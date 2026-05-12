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
} AppState;

static AppState app;
static Hdc2010  hdc2010_dev;

static void apply_throttle(SystemState state, const Settings *s)
{
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

static bool apply_fans(SystemState state, bool button_override)
{
    bool auto_on = (state == SystemHigh) || (state == SystemThrottling) || (state == SystemCritical);
    bool fans_on = button_override || auto_on;

    if (fans_on)
    {
        fan_control_all_on();
    }
    else
    {
        fan_control_all_off();
    }

    return fans_on;
}

static bool apply_lcd_power(SystemState state)
{
    bool lcd_on = (state != SystemCritical && state != SystemError);
    board_lcd_power_set(lcd_on);
    return lcd_on;
}

static void apply_program_led(SystemState state)
{
    ProgramLedState led_state;

    switch (state)
    {
        case SystemCritical:
            led_state = ProgramLedCritical;
            break;

        case SystemThrottling:
            led_state = ProgramLedThrottling;
            break;

        case SystemHigh:
            led_state = ProgramLedHigh;
            break;

        case SystemLow:
            led_state = ProgramLedLow;
            break;

        case SystemSensorLost:
        case SystemError:
        default:
            led_state = ProgramLedError;
            break;
    }

    program_led_set_state(led_state);
}

void app_set_mode(AppMode mode)
{
    app.mode = mode;
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

static void app_state_init(void)
{
    app.mode  = ModeNormal;
    app.state = SystemLow;
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
            bool button_override = push_button_is_pressed();

            apply_throttle(app.state, s);
            apply_fans(app.state, button_override);
            apply_lcd_power(app.state);
            apply_program_led(app.state);
            break;
        }

        case ModeManual:
        {
            /* Manual mode: external control via USB commands */
            /* Throttle: set via PWMTHR=<channel>,<duty> commands */
            /* Fans: set via FAN<n>=ON/OFF commands */
            /* Skip state machine processing; use commands for all control */

            apply_program_led(SystemLow);
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
