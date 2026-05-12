#include "app_mode.h"
#include "board.h"
#include "board/board_config.h"
#include "commands.h"
#include "hdc2010.h"
#include "delay.h"
#include "fan_control.h"
#include "fan_tacho.h"
#include "program_led.h"
#include "push_button.h"
#include "pwm_repeater.h"
#include "serial.h"
#include "settings.h"
#include "stm32c0xx_hal.h"
#include "telemetry.h"
#include "thermal_control.h"
#include "temperature_sensor.h"
#include "timers/timers.h"
#include "usb.h"
#include "watchdog.h"
#include <stdbool.h>
#include <stdint.h>

/* Tunables */
#define APP_FAN_PWM_FREQ_HZ      25000U
#define APP_TEMP_HYSTERESIS_CDEG 50U      /* setpoint hysteresis for temp_sensor module */
#define APP_FAN_COUNT            4U

typedef struct
{
    AppMode     mode;
    SystemState state;
    bool        button_override;
    bool        throttle_override_active;
    uint8_t     throttle_override_a;
    uint8_t     throttle_override_b;
} AppState;

static AppState app;
static Hdc2010  hdc2010_dev;

/* User throttle override (set via PWMTHR command) */
static bool    user_throttle_override_active = false;
static uint8_t user_throttle_override_a = 100U;
static uint8_t user_throttle_override_b = 100U;

static void apply_throttle(SystemState state, const Settings *s)
{
    if (user_throttle_override_active)
    {
        pwm_set_throttle_a(user_throttle_override_a);
        pwm_set_throttle_b(user_throttle_override_b);
        return;
    }

    switch (state)
    {
        case SystemCritical:
        case SystemError:
            pwm_set_throttle_a(0U);
            pwm_set_throttle_b(0U);
            break;

        case SystemThrottling:
            pwm_set_throttle_a(s->pwm_throttle_a);
            pwm_set_throttle_b(s->pwm_throttle_b);
            break;

        case SystemLow:
        case SystemHigh:
        case SystemSensorLost:
        default:
            pwm_set_throttle_a(100U);
            pwm_set_throttle_b(100U);
            break;
    }
}

static void apply_fans(bool fans_on)
{
    if (fans_on)
    {
        fan_control_all_on();
    }
    else
    {
        fan_control_all_off();
    }
}

static void update_led(SystemState state, bool fans_on)
{
    if (state == SystemSensorLost || state == SystemCritical || state == SystemError)
    {
        program_led_set_state(ProgramLedError);
    }
    else if (fans_on)
    {
        program_led_set_state(ProgramLedFansOn);
    }
    else
    {
        program_led_set_state(ProgramLedFansOff);
    }
}

void app_clear_throttle_override(void);

void app_set_mode(AppMode mode)
{
    if (app.mode != mode)
    {
        app.mode = mode;
        if (mode == ModeNormal)
        {
            app_clear_throttle_override();
        }
    }
}

AppMode app_get_mode(void)
{
    return app.mode;
}

void app_set_state(SystemState state)
{
    app.state = state;
}

SystemState app_get_state(void)
{
    return app.state;
}

void app_set_throttle_override(uint32_t throttle_a, uint32_t throttle_b)
{
    app.throttle_override_active = true;
    app.throttle_override_a = (uint8_t)((throttle_a > 100U) ? 100U : throttle_a);
    app.throttle_override_b = (uint8_t)((throttle_b > 100U) ? 100U : throttle_b);
    pwm_set_throttle_a(app.throttle_override_a);
    pwm_set_throttle_b(app.throttle_override_b);
}

void app_clear_throttle_override(void)
{
    app.throttle_override_active = false;
    app.throttle_override_a = 100U;
    app.throttle_override_b = 100U;
}

uint32_t app_get_throttle_override_a(void)
{
    return app.throttle_override_a;
}

uint32_t app_get_throttle_override_b(void)
{
    return app.throttle_override_b;
}

static void app_state_init(void)
{
    app.mode                      = ModeNormal;
    app.state                     = SystemLow;
    app.button_override           = false;
    app.throttle_override_active  = false;
    app.throttle_override_a       = 100U;
    app.throttle_override_b       = 100U;
}

static void app_task(void)
{
    const Settings *s = settings_get();
    if (s == NULL)
    {
        return;
    }

    switch (app.mode)
    {
        case ModeNormal:
        {
            /* Normal mode: system state machine controls behavior */
            app.state = thermal_control_step(app.state, s);
            app.button_override = push_button_is_pressed();

            bool fans_auto_on     = (app.state == SystemHigh) || (app.state == SystemThrottling) || (app.state == SystemCritical);
            bool fans_required_on = app.button_override || fans_auto_on;
            bool lcd_power_on     = (app.state != SystemCritical && app.state != SystemError);

            apply_throttle(app.state, s);
            apply_fans(fans_required_on);
            board_lcd_power_set(lcd_power_on);
            update_led(app.state, fans_required_on);
            break;
        }

        case ModeManual:
        {
            /* Manual mode: external control via USB commands */
            /* Throttle: set via PWMTHR=<channel>,<duty> commands */
            /* Fans: set via FAN<n>=ON/OFF commands */
            /* Skip state machine processing; use commands for all control */
            app.button_override = false;

            if (app.throttle_override_active)
            {
                pwm_set_throttle_a(app.throttle_override_a);
                pwm_set_throttle_b(app.throttle_override_b);
            }

            bool any_fan_on = false;
            for (uint8_t i = 1U; i <= APP_FAN_COUNT; i++)
            {
                if (fan_control_get_unit_duty(i) > 0U)
                {
                    any_fan_on = true;
                    break;
                }
            }

            update_led(SystemLow, any_fan_on);
            break;
        }
    }
}

int main(void)
{
    /* Phase 1: MCU + basic services */
    HAL_Init();
    board_init();
    timers_init();
    delay_init(timers_get_sys_timer());
    watchdog_init();
    serial_init(BOARD_UART1_INSTANCE, BOARD_UART1_BAUD_RATE);

    /* Phase 2: Persisted settings */
    settings_init();
    const Settings *s = settings_get();

    /* Phase 3: USB up early so enumeration can start; nothing below blocks */
    usb_init();
    telemetry_init();
    commands_init();
    hdc2010_init(&hdc2010_dev, board_get_i2c(), HDC2010_ADDR_LOW);

    /* Phase 4: I/O */
    program_led_init();
    push_button_init();

    pwm_repeater_init(timers_get_capture(), timers_get_repeater_a(), timers_get_repeater_b());
    pwm_set_throttle_a(100U);
    pwm_set_throttle_b(100U);

    board_onewire_power_set(true);
    board_onewire_pullup_set(true);
    temperature_sensor_init();
    temperature_sensor_set_setpoint_a((uint16_t)(s->temp_fan_off * 100U));
    temperature_sensor_set_setpoint_b((uint16_t)(s->temp_fan_on * 100U));
    temperature_sensor_set_hysteresis(APP_TEMP_HYSTERESIS_CDEG);

    fan_control_init(timers_get_fan_power(), timers_get_fan_remote());
    fan_init(APP_FAN_PWM_FREQ_HZ);

    /* Start with all fans off; thermal SM takes over from first iteration */
    fan_control_all_off();

    app_state_init();
    thermal_control_init();

    /* Phase 5: Cooperative main loop */
    while (true)
    {
        watchdog_kick();
        usb_task();
        commands_task();
        push_button_task();
        pwm_repeater_task();
        temperature_sensor_task();
        hdc2010_task();
        program_led_task();
        telemetry_task();

        app_task();
    }
}
