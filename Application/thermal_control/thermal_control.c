#include "thermal_control.h"
#include "system_temp.h"

#define CRITICAL_HYSTERESIS_CDEG  200     /* 2 C below T_critical to exit critical */
#define THROTTLE_HYSTERESIS_CDEG  200     /* 2 C below T_throttle_on to exit throttling */

static SystemState step(SystemState prev, uint16_t raw, const Settings *s);

/* ── Public API ──────────────────────────────────────────────────────────── */

void thermal_control_init(void)
{
}

SystemState thermal_control_step(SystemState current, const Settings *s)
{
    if (current == SystemError)
    {
        return SystemError;
    }

    return step(current, system_temp_get(), s);
}

/* ── Static functions ────────────────────────────────────────────────────── */

static SystemState step(SystemState prev, uint16_t raw, const Settings *s)
{
    if (raw == 0xFFFFU)
    {
        return SystemSensorLost;
    }

    int16_t t_cdeg       = (int16_t)raw;
    int16_t t_deg        = t_cdeg / 100;
    int16_t crit_on      = (int16_t)s->temp_critical;
    int16_t crit_off     = crit_on - (CRITICAL_HYSTERESIS_CDEG / 100);
    int16_t throttle_on  = (int16_t)s->temp_throttle_on;
    int16_t throttle_off = throttle_on - (THROTTLE_HYSTERESIS_CDEG / 100);
    int16_t fan_on       = (int16_t)s->temp_fan_on;
    int16_t fan_off      = (int16_t)s->temp_fan_off;

    switch (prev)
    {
        case SystemSensorLost:
            /* Re-enter normal SM as if from Low */
            if (t_deg >= crit_on)
            {
                return SystemCritical;
            }
            if (t_deg >= throttle_on)
            {
                return SystemThrottling;
            }
            if (t_deg >= fan_on)
            {
                return SystemHigh;
            }
            return SystemLow;

        case SystemLow:
            if (t_deg >= crit_on)
            {
                return SystemCritical;
            }
            if (t_deg >= throttle_on)
            {
                return SystemThrottling;
            }
            if (t_deg >= fan_on)
            {
                return SystemHigh;
            }
            return SystemLow;

        case SystemHigh:
            if (t_deg >= crit_on)
            {
                return SystemCritical;
            }
            if (t_deg >= throttle_on)
            {
                return SystemThrottling;
            }
            if (t_deg < fan_off)
            {
                return SystemLow;
            }
            return SystemHigh;

        case SystemThrottling:
            if (t_deg >= crit_on)
            {
                return SystemCritical;
            }
            if (t_deg < throttle_off)
            {
                return SystemHigh;
            }
            return SystemThrottling;

        case SystemCritical:
            if (t_deg < crit_off)
            {
                return SystemThrottling;
            }
            return SystemCritical;

        case SystemError:
            return SystemError;
    }
    return SystemLow;
}
