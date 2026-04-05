#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <stdint.h>

typedef struct
{
    uint8_t power_channel;
    uint8_t remote_channel;
} FanLink;

void fan_control_init(void);
void fan_power_init(uint32_t frequency_hz);
void fan_remote_init(uint32_t frequency_hz);
void fan_init(uint32_t frequency_hz);

void fan_control_set_power_channel_duty(uint8_t channel, uint8_t duty_pct);
void fan_control_set_remote_channel_duty(uint8_t channel, uint8_t duty_pct);
uint8_t fan_control_get_power_channel_duty(uint8_t channel);
uint8_t fan_control_get_remote_channel_duty(uint8_t channel);

void fan_control_set_unit_duty(uint8_t unit_idx, uint8_t duty_pct);

#endif /* FAN_CONTROL_H */
