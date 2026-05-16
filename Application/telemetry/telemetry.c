#include "telemetry.h"
#include "config.h"
#include "temperature_sensor.h"
#include "pwm_repeater.h"
#include "fan_control.h"
#include "sys_time.h"
#include "usb.h"
#include "hdc2010.h"
#include "push_button.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define TELEMETRY_BUF_SIZE 128U

typedef struct
{
    bool enabled;
    millis_t last_send_ms;
} Telemetry;

static Telemetry telemetry = {
    .enabled = true,
    .last_send_ms = 0U,
};

static int cdeg_to_deg_rounded(int16_t cdeg)
{
    return (cdeg >= 0) ? (cdeg + 50) / 100 : (cdeg - 50) / 100;
}

static const char *thermal_state_to_string(ThermalState thermal)
{
    switch (thermal)
    {
        case ThermalLow:
            return "TEMP_LOW";
        case ThermalHigh:
            return "TEMP_HIGH";
        case ThermalThrottling:
            return "TEMP_THROTTLE";
        case ThermalCritical:
            return "TEMP_CRIT";
        default:
            return "UNKNOWN";
    }
}

static const char *system_state_to_string(SystemState state, ThermalState thermal)
{
    switch (state)
    {
        case SystemBoot:
            return "BOOT";
        case SystemFault:
            return "FAULT";
        case SystemRunning:
            return thermal_state_to_string(thermal);
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
    telemetry.last_send_ms = 0U;
}

void telemetry_create(char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0U)
    {
        return;
    }

    int16_t raw_temp = get_temperature();
    int16_t hdc_temp = hdc2010_get_temp();
    uint8_t hdc_rh = hdc2010_get_rh();
    uint32_t boot_s = (uint32_t)(millis() / 1000U);
    const char *fan_str = get_fan_state_str();
    uint32_t in_dc_a = pwm_get_duty_a();
    uint32_t out_dc_a = pwm_get_output_duty_a();
    uint32_t in_dc_b = pwm_get_duty_b();
    uint32_t out_dc_b = pwm_get_output_duty_b();

    SystemState state = app_get_state();
    ThermalState thermal = app_get_thermal_state();
    const char *state_str = system_state_to_string(state, thermal);
    const char *btn_str = push_button_is_pressed() ? "1" : "0";

    char ds_temp_str[8];
    if (raw_temp == INT16_MIN)
    {
        snprintf(ds_temp_str, sizeof(ds_temp_str), "ERR");
    }
    else
    {
        snprintf(ds_temp_str, sizeof(ds_temp_str), "%d", cdeg_to_deg_rounded(raw_temp));
    }

    char hdc_temp_str[8];
    char hdc_rh_str[4];
    if (hdc_temp == INT16_MIN)
    {
        snprintf(hdc_temp_str, sizeof(hdc_temp_str), "ERR");
        snprintf(hdc_rh_str, sizeof(hdc_rh_str), "FF");
    }
    else
    {
        snprintf(hdc_temp_str, sizeof(hdc_temp_str), "%d", cdeg_to_deg_rounded(hdc_temp));
        snprintf(hdc_rh_str, sizeof(hdc_rh_str), "%u", (unsigned)hdc_rh);
    }

    snprintf(buf, buf_size, "$01,%lu,%s,%s,%s,%s,%lu,%lu,%lu,%lu,%s,%s\r\n",
        (unsigned long)boot_s,
        ds_temp_str,
        hdc_temp_str,
        hdc_rh_str,
        fan_str,
        (unsigned long)in_dc_a,
        (unsigned long)out_dc_a,
        (unsigned long)in_dc_b,
        (unsigned long)out_dc_b,
        state_str,
        btn_str);
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
    millis_t now = millis();
    if ((now - telemetry.last_send_ms) >= (millis_t)CONFIG_TELEMETRY_INTERVAL_MS)
    {
        telemetry.last_send_ms = now;
        telemetry_send();
    }
}

void telemetry_reset(void)
{
    telemetry.enabled = true;
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

uint32_t telemetry_get_interval_ms(void)
{
    return CONFIG_TELEMETRY_INTERVAL_MS;
}
