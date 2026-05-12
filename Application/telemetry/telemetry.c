#include "telemetry.h"
#include "temperature_sensor.h"
#include "pwm_repeater.h"
#include "fan_control.h"
#include "sys_time.h"
#include "usb.h"
#include "hdc2010.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct
{
    bool enabled;
    uint32_t interval_ms;
    uint64_t last_send_ms;
} Telemetry;

static Telemetry telemetry = {
    .enabled = true,
    .interval_ms = TELEMETRY_DEFAULT_INTERVAL_MS,
    .last_send_ms = 0U,
};

static const char *system_state_to_string(SystemState state)
{
    switch (state)
    {
        case SystemLow:
            return "TEMP_LOW";
        case SystemHigh:
            return "TEMP_HIGH";
        case SystemThrottling:
            return "TEMP_THROTTLE";
        case SystemCritical:
            return "TEMP_CRIT";
        case SystemSensorLost:
            return "SENSOR_LOST";
        case SystemError:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

static const char *get_fan_state_str(void)
{
    static char buf[5];
    buf[0] = fan_control_get_power_channel_duty(FanChannelOne) > 0U ? '1' : '0';
    buf[1] = fan_control_get_power_channel_duty(FanChannelTwo) > 0U ? '1' : '0';
    buf[2] = fan_control_get_power_channel_duty(FanChannelThree) > 0U ? '1' : '0';
    buf[3] = fan_control_get_power_channel_duty(FanChannelFour) > 0U ? '1' : '0';
    buf[4] = '\0';
    return buf;
}

void telemetry_init(void)
{
    telemetry.enabled = true;
    telemetry.interval_ms = TELEMETRY_DEFAULT_INTERVAL_MS;
    telemetry.last_send_ms = 0U;
}

void telemetry_create(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0U)
    {
        return;
    }

    uint16_t raw_temp = get_temperature();
    uint16_t hdc_temp = hdc2010_get_temp();
    uint8_t hdc_rh = hdc2010_get_rh();
    uint32_t boot_s = (uint32_t)(millis() / 1000U);
    const char *fan_str = get_fan_state_str();
    uint32_t in_dc_a = pwm_get_duty_a();
    uint32_t out_dc_a = pwm_get_output_duty_a();
    uint32_t in_dc_b = pwm_get_duty_b();
    uint32_t out_dc_b = pwm_get_output_duty_b();

    SystemState state = app_get_state();
    const char *state_str = system_state_to_string(state);

    char ds_temp_str[8];
    if (raw_temp == 0xFFFFU)
    {
        snprintf(ds_temp_str, sizeof(ds_temp_str), "ERR");
    }
    else
    {
        snprintf(ds_temp_str, sizeof(ds_temp_str), "%d", (int)((int16_t)raw_temp / 100));
    }

    char hdc_temp_str[8];
    char hdc_rh_str[4];
    if (hdc_temp == 0xFFFFU)
    {
        snprintf(hdc_temp_str, sizeof(hdc_temp_str), "ERR");
        snprintf(hdc_rh_str, sizeof(hdc_rh_str), "FF");
    }
    else
    {
        snprintf(hdc_temp_str, sizeof(hdc_temp_str), "%d", (int)((int16_t)hdc_temp / 100));
        snprintf(hdc_rh_str, sizeof(hdc_rh_str), "%u", (unsigned)hdc_rh);
    }

    snprintf(buf, buf_size, "$01,%lu,%s,%s,%s,%s,%lu,%lu,%lu,%lu,%s\r\n",
        (unsigned long)boot_s,
        ds_temp_str,
        hdc_temp_str,
        hdc_rh_str,
        fan_str,
        (unsigned long)in_dc_a,
        (unsigned long)out_dc_a,
        (unsigned long)in_dc_b,
        (unsigned long)out_dc_b,
        state_str);
}

void telemetry_send(void)
{
    char buf[TELEMETRY_BUF_SIZE];
    telemetry_create(buf, sizeof(buf));
    usb_printf("%s", buf);
}

void telemetry_task(void)
{
    if (!telemetry.enabled)
    {
        return;
    }
    uint64_t now = millis();
    if ((now - telemetry.last_send_ms) >= (uint64_t)telemetry.interval_ms)
    {
        telemetry.last_send_ms = now;
        telemetry_send();
    }
}

void telemetry_reset(void)
{
    telemetry.enabled = true;
    telemetry.interval_ms = TELEMETRY_DEFAULT_INTERVAL_MS;
    telemetry.last_send_ms = 0U;
}

void telemetry_enable(bool enable)
{
    telemetry.enabled = enable;
}

bool telemetry_is_enabled(void)
{
    return telemetry.enabled;
}

void telemetry_set_interval_ms(uint32_t ms)
{
    if (ms < TELEMETRY_MIN_INTERVAL_MS)
    {
        ms = TELEMETRY_MIN_INTERVAL_MS;
    }
    if (ms > TELEMETRY_MAX_INTERVAL_MS)
    {
        ms = TELEMETRY_MAX_INTERVAL_MS;
    }
    telemetry.interval_ms = ms;
}

uint32_t telemetry_get_interval_ms(void)
{
    return telemetry.interval_ms;
}
