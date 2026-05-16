#include "thermal_control.h"

#define CRITICAL_HYSTERESIS_DEG  2     /* degrees below T_critical to exit critical */
#define THROTTLE_HYSTERESIS_DEG  2     /* degrees below T_throttle_on to exit throttling */

typedef struct
{
    int16_t t_deg;
    int16_t crit_on;
    int16_t crit_off;
    int16_t throttle_on;
    int16_t throttle_off;
    int16_t fan_on;
    int16_t fan_off;
} Thresholds;

static Thresholds compute_thresholds(int16_t t_cdeg, const Settings *s);

void thermal_control_init(void)
{
}

ThermalState thermal_control_initial(int16_t t_cdeg, const Settings *s)
{
    Thresholds th = compute_thresholds(t_cdeg, s);

    if (th.t_deg >= th.crit_on)
    {
        return ThermalCritical;
    }
    if (th.t_deg >= th.throttle_on)
    {
        return ThermalThrottling;
    }
    if (th.t_deg >= th.fan_on)
    {
        return ThermalHigh;
    }
    return ThermalLow;
}

ThermalState thermal_control_step(ThermalState current, int16_t t_cdeg, const Settings *s)
{
    Thresholds th = compute_thresholds(t_cdeg, s);

    switch (current)
    {
        case ThermalLow:
            if (th.t_deg >= th.crit_on)
            {
                return ThermalCritical;
            }
            if (th.t_deg >= th.throttle_on)
            {
                return ThermalThrottling;
            }
            if (th.t_deg >= th.fan_on)
            {
                return ThermalHigh;
            }
            return ThermalLow;

        case ThermalHigh:
            if (th.t_deg >= th.crit_on)
            {
                return ThermalCritical;
            }
            if (th.t_deg >= th.throttle_on)
            {
                return ThermalThrottling;
            }
            if (th.t_deg < th.fan_off)
            {
                return ThermalLow;
            }
            return ThermalHigh;

        case ThermalThrottling:
            if (th.t_deg >= th.crit_on)
            {
                return ThermalCritical;
            }
            if (th.t_deg < th.throttle_off)
            {
                return ThermalHigh;
            }
            return ThermalThrottling;

        case ThermalCritical:
            if (th.t_deg < th.crit_off)
            {
                return ThermalThrottling;
            }
            return ThermalCritical;

        default:
            return ThermalLow;
    }
}

static Thresholds compute_thresholds(int16_t t_cdeg, const Settings *s)
{
    Thresholds th;
    th.t_deg        = t_cdeg / 100;
    th.crit_on      = (int16_t)s->temp_critical;
    th.crit_off     = th.crit_on - CRITICAL_HYSTERESIS_DEG;
    th.throttle_on  = (int16_t)s->temp_throttle_on;
    th.throttle_off = th.throttle_on - THROTTLE_HYSTERESIS_DEG;
    th.fan_on       = (int16_t)s->temp_fan_on;
    th.fan_off      = (int16_t)s->temp_fan_off;
    return th;
}
