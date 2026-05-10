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
#include "system_temp.h"
#include "telemetry.h"
#include "temperature_sensor.h"
#include "timers/timers.h"
#include "usb.h"
#include "watchdog.h"
#include <stdbool.h>
#include <stdint.h>

/* Tunables */
#define APP_FAN_PWM_FREQ_HZ          25000U
#define APP_TEMP_HYSTERESIS_CDEG     50U      /* setpoint hysteresis for temp_sensor module */
#define APP_CRITICAL_HYSTERESIS_CDEG 200      /* +/-2 C around T_critical */
#define APP_FAN_COUNT                4U

typedef enum
{
    ThermalLow,
    ThermalHigh,
    ThermalCritical,
    ThermalSensorLost,
} ThermalState;

typedef struct
{
    AppMode      mode;
    ThermalState thermal;
    bool         button_override;
    bool         throttle_override_active;
    uint32_t     throttle_override_a;
    uint32_t     throttle_override_b;
} AppState;

static AppState app;
static Hdc2010  hdc2010_dev;
static bool     hdc2010_ok;
static bool     hdc2010_valid;
static int16_t  hdc2010_last_temp;
static uint8_t  hdc2010_last_rh;

/* User throttle override (set via PWMTHR command) */
static bool     user_throttle_override_active = false;
static uint32_t user_throttle_override_a = 100U;
static uint32_t user_throttle_override_b = 100U;

static ThermalState thermal_step(ThermalState prev, uint16_t raw, const Settings *s)
{
    if (raw == 0xFFFFU)
    {
        return ThermalSensorLost;
    }

    int16_t t_cdeg   = (int16_t)raw;
    int16_t t_deg    = t_cdeg / 100;
    int16_t crit_on  = (int16_t)s->temp_critical;
    int16_t crit_off = crit_on - (APP_CRITICAL_HYSTERESIS_CDEG / 100);
    int16_t fan_on   = (int16_t)s->temp_fan_on;
    int16_t fan_off  = (int16_t)s->temp_fan_off;

    switch (prev)
    {
        case ThermalSensorLost:
            /* Re-enter normal SM as if from Low */
            if (t_deg >= crit_on)
            {
                return ThermalCritical;
            }
            if (t_deg >= fan_on)
            {
                return ThermalHigh;
            }
            return ThermalLow;

        case ThermalLow:
            if (t_deg >= crit_on)
            {
                return ThermalCritical;
            }
            if (t_deg >= fan_on)
            {
                return ThermalHigh;
            }
            return ThermalLow;

        case ThermalHigh:
            if (t_deg >= crit_on)
            {
                return ThermalCritical;
            }
            if (t_deg < fan_off)
            {
                return ThermalLow;
            }
            return ThermalHigh;

        case ThermalCritical:
            if (t_deg < crit_off)
            {
                return ThermalHigh;
            }
            return ThermalCritical;
    }
    return ThermalLow;
}

static void apply_throttle(ThermalState state, const Settings *s)
{
    if (user_throttle_override_active)
    {
        pwm_set_throttle_a(user_throttle_override_a);
        pwm_set_throttle_b(user_throttle_override_b);
        return;
    }

    switch (state)
    {
        case ThermalCritical:
            pwm_set_throttle_a(0U);
            pwm_set_throttle_b(0U);
            break;

        case ThermalHigh:
        case ThermalSensorLost:
            pwm_set_throttle_a((uint32_t)s->pwm_throttle_a);
            pwm_set_throttle_b((uint32_t)s->pwm_throttle_b);
            break;

        case ThermalLow:
        default:
            pwm_set_throttle_a(100U);
            pwm_set_throttle_b(100U);
            break;
    }
}

static void apply_fans(bool fans_on)
{
    uint8_t duty = fans_on ? 100U : 0U;
    for (uint8_t i = 1U; i <= APP_FAN_COUNT; i++)
    {
        fan_control_set_unit_duty(i, duty);
    }
}

static void update_led(ThermalState state, bool fans_on)
{
    if (state == ThermalSensorLost || state == ThermalCritical)
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

void app_set_throttle_override(uint32_t throttle_a, uint32_t throttle_b)
{
    app.throttle_override_active = true;
    app.throttle_override_a = (throttle_a > 100U) ? 100U : throttle_a;
    app.throttle_override_b = (throttle_b > 100U) ? 100U : throttle_b;
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

static void hdc2010_poll_task(void)
{
    typedef enum { Hdc2010Idle, Hdc2010Waiting } Hdc2010PollState;
    static Hdc2010PollState state      = Hdc2010Idle;
    static uint32_t         trigger_ms = 0U;
    static uint32_t         poll_ms    = 0U;

    if (!hdc2010_ok)
    {
        usb_printf("HDC2010 not detected; skipping environmental sensor polling\r\n");
        return;
    }

    uint32_t now = HAL_GetTick();

    switch (state)
    {
        case Hdc2010Idle:
            if (now - poll_ms >= 1000U)
            {
                hdc2010_start_measurement(&hdc2010_dev);
                trigger_ms = now;
                state      = Hdc2010Waiting;
            }
            break;

        case Hdc2010Waiting:
            if (now - trigger_ms >= 2U)
            {
                int16_t temp = 0;
                uint8_t rh   = 0;
                if (hdc2010_read(&hdc2010_dev, &temp, &rh) == HDC2010_OK)
                {
                    hdc2010_last_temp = temp;
                    hdc2010_last_rh   = rh;
                    hdc2010_valid     = true;
                }
                poll_ms = now;
                state   = Hdc2010Idle;
            }
            break;
    }
}

static void app_state_init(void)
{
    app.mode                      = ModeNormal;
    app.thermal                   = ThermalLow;
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

    uint32_t now_ms = HAL_GetTick();

    switch (app.mode)
    {
        case ModeNormal:
        {
            /* Normal mode: thermal state machine controls behavior */
            app.thermal         = thermal_step(app.thermal, system_temp_get(), s);
            app.button_override = push_button_is_pressed();

            bool fans_auto_on     = (app.thermal == ThermalHigh) || (app.thermal == ThermalCritical);
            bool fans_required_on = app.button_override || fans_auto_on;

            apply_throttle(app.thermal, s);
            apply_fans(fans_required_on);
            update_led(app.thermal, fans_required_on);
            break;
        }

        case ModeManual:
        {
            /* Manual mode: external control via USB commands */
            /* Throttle: set via PWMTHR=<channel>,<duty> commands */
            /* Fans: set via FAN<n>=ON/OFF commands */
            /* Skip thermal state machine processing; use commands for all control */
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

            update_led(ThermalLow, any_fan_on);
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
    hdc2010_ok = (hdc2010_init(&hdc2010_dev, board_get_i2c(), HDC2010_ADDR_LOW) == HDC2010_OK);

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
    for (uint8_t i = 1U; i <= APP_FAN_COUNT; i++)
    {
        fan_control_set_unit_duty(i, 0U);
    }

    app_state_init();

    /* Phase 5: Cooperative main loop */
    while (true)
    {
        watchdog_kick();
        usb_task();
        commands_task();
        push_button_task();
        pwm_repeater_task();
        temperature_sensor_task();
        hdc2010_poll_task();
        program_led_task();
        telemetry_task();

        app_task();
    }
}
