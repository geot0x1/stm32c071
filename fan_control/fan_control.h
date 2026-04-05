#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <stdint.h>

typedef enum
{
    FanChannel1 = 1,
    FanChannel2,
    FanChannel3,
    FanChannel4
} FanChannel;

void fan_control_init(uint32_t frequency_hz);
void fan_control_set_duty(FanChannel channel, uint8_t duty_pct);
uint8_t fan_control_get_duty(FanChannel channel);

#endif /* FAN_CONTROL_H */
