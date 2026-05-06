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
#include "temperature_sensor.h"
#include "timers/timers.h"
#include "usb.h"
#include "watchdog.h"
#include <stdbool.h>
#include <stdint.h>

/* Tunables */
#define APP_DEBUG_ENABLE                 1        /* set to 0 to silence all [INIT]/[STATUS] output */
#define APP_FAN_CYCLE_TEST_ENABLE        1        /* set to 1 to enable 2-second fan cycle test */
#define APP_FAN_PWM_FREQ_HZ              25000U
#define APP_TEMP_HYSTERESIS_CDEG         50U      /* setpoint hysteresis for temp_sensor module */
#define APP_CRITICAL_HYSTERESIS_CDEG     200      /* +/-2 C around T_critical */
#define APP_PRESENCE_SAMPLE_MS           100U
#define APP_RPM_PRESENT_THRESHOLD        100U
#define APP_PRESENCE_MISSING_DEBOUNCE_MS 1500U
#define APP_FAN_COUNT                    4U

typedef enum
{
    ThermalLow,
    ThermalHigh,
    ThermalCritical,
    ThermalSensorLost,
} ThermalState;

typedef struct
{
    ThermalState thermal;
    bool         button_override;
    bool         critical_throttle_active;
    bool         fan_present[APP_FAN_COUNT];
    uint32_t     missing_since_ms[APP_FAN_COUNT];
    uint32_t     last_presence_sample_ms;
} AppState;

static AppState app;
static Hdc2010  hdc2010_dev;
static bool     hdc2010_ok;

static ThermalState thermal_step(ThermalState prev, uint16_t raw, const Settings *s)
{
    if (raw == 0xFFFFU)
    {
        return ThermalSensorLost;
    }

    int16_t t        = (int16_t)raw;
    int16_t crit_on  = s->temp_critical;
    int16_t crit_off = (int16_t)(s->temp_critical - APP_CRITICAL_HYSTERESIS_CDEG);
    int16_t fan_on   = s->temp_fan_on;
    int16_t fan_off  = s->temp_fan_off;

    switch (prev)
    {
        case ThermalSensorLost:
            /* Re-enter normal SM as if from Low */
            if (t >= crit_on)
            {
                return ThermalCritical;
            }
            if (t >= fan_on)
            {
                return ThermalHigh;
            }
            return ThermalLow;

        case ThermalLow:
            if (t >= crit_on)
            {
                return ThermalCritical;
            }
            if (t >= fan_on)
            {
                return ThermalHigh;
            }
            return ThermalLow;

        case ThermalHigh:
            if (t >= crit_on)
            {
                return ThermalCritical;
            }
            if (t < fan_off)
            {
                return ThermalLow;
            }
            return ThermalHigh;

        case ThermalCritical:
            if (t < crit_off)
            {
                return ThermalHigh;
            }
            return ThermalCritical;
    }
    return ThermalLow;
}

static void apply_throttle(ThermalState state, const Settings *s)
{
    if (state == ThermalCritical)
    {
        if (!app.critical_throttle_active)
        {
            pwm_set_throttle_a(0U);
            pwm_set_throttle_b(0U);
            app.critical_throttle_active = true;
        }
    }
    else
    {
        if (app.critical_throttle_active)
        {
            pwm_set_throttle_a((uint32_t)s->pwm_throttle_a);
            pwm_set_throttle_b((uint32_t)s->pwm_throttle_b);
            app.critical_throttle_active = false;
        }
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

static void update_fan_presence(uint32_t now_ms)
{
    if (now_ms - app.last_presence_sample_ms < APP_PRESENCE_SAMPLE_MS)
    {
        return;
    }
    app.last_presence_sample_ms = now_ms;

    for (uint8_t i = 0U; i < APP_FAN_COUNT; i++)
    {
        uint8_t unit = i + 1U;

        if (fan_control_get_type(unit) == FanType2Wire)
        {
            /* No tacho available — assume present */
            app.fan_present[i]      = true;
            app.missing_since_ms[i] = 0U;
            continue;
        }

        bool commanded_on = (fan_control_get_unit_duty(unit) > 0U);
        if (!commanded_on)
        {
            app.missing_since_ms[i] = 0U;
            continue;
        }

        uint32_t rpm = fan_tacho_get_rpm(unit);
        if (rpm >= APP_RPM_PRESENT_THRESHOLD)
        {
            app.fan_present[i]      = true;
            app.missing_since_ms[i] = 0U;
        }
        else
        {
            if (app.missing_since_ms[i] == 0U)
            {
                app.missing_since_ms[i] = now_ms;
            }
            else if (now_ms - app.missing_since_ms[i] >= APP_PRESENCE_MISSING_DEBOUNCE_MS)
            {
                app.fan_present[i] = false;
            }
        }
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

#if APP_DEBUG_ENABLE
static const char *thermal_state_str(ThermalState state)
{
    switch (state)
    {
        case ThermalLow:        return "LOW";
        case ThermalHigh:       return "HIGH";
        case ThermalCritical:   return "CRITICAL";
        case ThermalSensorLost: return "SENSOR_LOST";
        default:                return "UNKNOWN";
    }
}

static void debug_task(void)
{
    static uint32_t last_ms = 0U;
    uint32_t now = HAL_GetTick();
    if (now - last_ms < 1000U)
    {
        return;
    }
    last_ms = now;

    // uint16_t raw_temp = get_temperature();
    // if (raw_temp == 0xFFFFU)
    // {
    //     serial_printf("[STATUS] Temp: LOST | Thermal: %s | BtnOverride: %s\r\n",
    //                   thermal_state_str(app.thermal),
    //                   app.button_override ? "YES" : "NO");
    // }
    // else
    // {
    //     int16_t t      = (int16_t)raw_temp;
    //     int16_t t_deg  = t / 100;
    //     int16_t t_frac = t % 100;
    //     if (t_frac < 0)
    //     {
    //         t_frac = -t_frac;
    //     }
    //     serial_printf("[STATUS] Temp: %d.%02d C | Thermal: %s | BtnOverride: %s\r\n",
    //                   t_deg, t_frac,
    //                   thermal_state_str(app.thermal),
    //                   app.button_override ? "YES" : "NO");
    // }

    // serial_printf("[STATUS] PWM-A: freq=%lu Hz in=%lu%% out=%lu%% throttle=%lu%%\r\n",
    //               pwm_get_frequency_a(), pwm_get_duty_a(),
    //               pwm_get_output_duty_a(), pwmOutputA.throttle_val);
    // serial_printf("[STATUS] PWM-B: freq=%lu Hz in=%lu%% out=%lu%% throttle=%lu%%\r\n",
    //               pwm_get_frequency_b(), pwm_get_duty_b(),
    //               pwm_get_output_duty_b(), pwmOutputB.throttle_val);

    // for (uint8_t i = 0U; i < APP_FAN_COUNT; i++)
    // {
    //     uint8_t     unit     = i + 1U;
    //     const char *type_str = (fan_control_get_type(unit) == FanType2Wire) ? "2W" : "3/4W";
    //     uint8_t     duty     = fan_control_get_unit_duty(unit);
    //     uint32_t    rpm      = fan_tacho_get_rpm(unit);
    //     serial_printf("[STATUS] Fan%u: type=%s duty=%u%% present=%s rpm=%lu\r\n",
    //                   unit, type_str, duty,
    //                   app.fan_present[i] ? "YES" : "NO",
    //                   rpm);
    // }

    if (hdc2010_ok)
    {
        hdc2010_start_measurement(&hdc2010_dev);
        HAL_Delay(2);
        int16_t temp_cdeg = 0;
        uint8_t rh        = 0;
        if (hdc2010_read(&hdc2010_dev, &temp_cdeg, &rh) == HDC2010_OK)
        {
            int16_t t_deg  = temp_cdeg / 100;
            int16_t t_frac = temp_cdeg % 100;
            if (t_frac < 0)
            {
                t_frac = -t_frac;
            }
            usb_printf("[HDC2010] Temp: %d.%02d C | RH: %u%%\r\n", t_deg, t_frac, rh);
        }
        else
        {
            usb_printf("[HDC2010] Read error\r\n");
        }
    }
    else
    {
        usb_printf("[HDC2010] Not present\r\n");
    }
}
#endif /* APP_DEBUG_ENABLE */

#if APP_FAN_CYCLE_TEST_ENABLE
static void fan_cycle_test(void)
{
    static uint32_t last_toggle_ms = 0U;
    static bool     fans_on        = false;

    uint32_t now = HAL_GetTick();
    if (now - last_toggle_ms < 2000U)
    {
        return;
    }
    last_toggle_ms = now;

    fans_on = !fans_on;
    uint8_t duty = fans_on ? 100U : 0U;
    for (uint8_t i = 1U; i <= APP_FAN_COUNT; i++)
    {
        fan_control_set_unit_duty(i, duty);
    }
    serial_printf("[TEST] Fans: %s\r\n", fans_on ? "ON" : "OFF");
}
#endif

static void app_state_init(void)
{
    app.thermal                  = ThermalLow;
    app.button_override          = false;
    app.critical_throttle_active = false;
    app.last_presence_sample_ms  = 0U;
    for (uint8_t i = 0U; i < APP_FAN_COUNT; i++)
    {
        app.fan_present[i]      = true; /* assume present until proven otherwise */
        app.missing_since_ms[i] = 0U;
    }
}

static void app_task(void)
{
    const Settings *s = settings_get();
    if (s == NULL)
    {
        return;
    }

    uint32_t now_ms = HAL_GetTick();

    app.thermal         = thermal_step(app.thermal, get_temperature(), s);
    app.button_override = push_button_is_pressed();

    bool fans_auto_on     = (app.thermal == ThermalHigh) || (app.thermal == ThermalCritical);
    bool fans_required_on = app.button_override || fans_auto_on;

    apply_throttle(app.thermal, s);
    apply_fans(fans_required_on);
    update_fan_presence(now_ms);
    update_led(app.thermal, fans_required_on);
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
#if APP_DEBUG_ENABLE
    serial_printf("[INIT] Phase 1: MCU + basic services OK\r\n");
#endif

    /* Phase 2: Persisted settings */
    settings_init();
    const Settings *s = settings_get();
#if APP_DEBUG_ENABLE
    serial_printf("[INIT] Phase 2: Settings loaded — throttle_a=%u%% throttle_b=%u%%"
                  " fan_on=%d fan_off=%d critical=%d (centideg)\r\n",
                  s->pwm_throttle_a, s->pwm_throttle_b,
                  s->temp_fan_on, s->temp_fan_off, s->temp_critical);
#endif

    /* Phase 3: USB up early so enumeration can start; nothing below blocks */
    usb_init();
    telemetry_init();
    commands_init();
    hdc2010_ok = (hdc2010_init(&hdc2010_dev, board_get_i2c(), HDC2010_ADDR_LOW) == HDC2010_OK);
#if APP_DEBUG_ENABLE
    serial_printf("[INIT] Phase 3: USB + telemetry + commands OK\r\n");
    serial_printf("[INIT] HDC2010: %s\r\n", hdc2010_ok ? "OK" : "NOT FOUND");
#endif

    /* Phase 4: I/O */
    program_led_init();
    push_button_init();
#if APP_DEBUG_ENABLE
    serial_printf("[INIT] Phase 4: LED + button OK\r\n");
#endif

    pwm_repeater_init(timers_get_capture(), timers_get_repeater_a(), timers_get_repeater_b());
    pwm_set_throttle_a((uint32_t)s->pwm_throttle_a);
    pwm_set_throttle_b((uint32_t)s->pwm_throttle_b);
#if APP_DEBUG_ENABLE
    serial_printf("[INIT] PWM repeater OK — throttle_a=%u%% throttle_b=%u%%\r\n",
                  s->pwm_throttle_a, s->pwm_throttle_b);
#endif

    board_onewire_power_set(true);
    board_onewire_pullup_set(true);
    temperature_sensor_init();
    temperature_sensor_set_setpoint_a(s->temp_fan_off);
    temperature_sensor_set_setpoint_b(s->temp_fan_on);
    temperature_sensor_set_hysteresis(APP_TEMP_HYSTERESIS_CDEG);
#if APP_DEBUG_ENABLE
    serial_printf("[INIT] Temp sensor OK — setpoint_a=%d setpoint_b=%d hyst=%u (centideg)\r\n",
                  s->temp_fan_off, s->temp_fan_on, APP_TEMP_HYSTERESIS_CDEG);
#endif

    for (uint8_t i = 1U; i <= APP_FAN_COUNT; i++)
    {
        fan_tacho_init(i);
        fan_tacho_enable(i);
    }
#if APP_DEBUG_ENABLE
    serial_printf("[INIT] Fan tacho OK — %u channels enabled\r\n", APP_FAN_COUNT);
#endif

    fan_control_init(timers_get_fan_power(), timers_get_fan_remote());
    fan_init(APP_FAN_PWM_FREQ_HZ);
#if APP_DEBUG_ENABLE
    serial_printf("[INIT] Fan control OK — PWM freq=%u Hz\r\n", APP_FAN_PWM_FREQ_HZ);
#endif

    /* Start with all fans off; thermal SM takes over from first iteration */
    for (uint8_t i = 1U; i <= APP_FAN_COUNT; i++)
    {
        fan_control_set_unit_duty(i, 0U);
    }

    app_state_init();
#if APP_DEBUG_ENABLE
    serial_printf("[INIT] App state init OK — all fans off, thermal=LOW\r\n");
    serial_printf("[INIT] Boot complete. Entering main loop.\r\n");
#endif

    /* Phase 5: Cooperative main loop */
    while (true)
    {
        watchdog_kick();
        usb_task();
        commands_task();
        push_button_task();
        pwm_repeater_task();
        temperature_sensor_task();
        program_led_task();
        telemetry_task();

#if APP_FAN_CYCLE_TEST_ENABLE
        fan_cycle_test();
#else
        app_task();
#endif
#if APP_DEBUG_ENABLE
        debug_task();
#endif
    }
}
