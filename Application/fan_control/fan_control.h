#ifndef FAN_CONTROL_H
#define FAN_CONTROL_H

#include <stdint.h>
#include "tim.h"

typedef enum
{
    FanChannelOne = 1,
    FanChannelTwo,
    FanChannelThree,
    FanChannelFour
} FanChannel;

/**
 * @brief Initialize fan control and store timer handles.
 *
 * @param power_tim  TIM1 handle from board_get_fan_power_tim()
 * @param remote_tim TIM3 handle from board_get_fan_remote_tim()
 */
void fan_control_init(Tim *power_tim, Tim *remote_tim);

void fan_power_init(uint32_t frequency_hz);
void fan_remote_init(uint32_t frequency_hz);
void fan_init(uint32_t frequency_hz);

void fan_control_set_power_channel_duty(FanChannel channel, uint8_t duty_pct);
void fan_control_set_remote_channel_duty(FanChannel channel, uint8_t duty_pct);
uint8_t fan_control_get_power_channel_duty(FanChannel channel);
uint8_t fan_control_get_remote_channel_duty(FanChannel channel);

void fan_control_set_unit_duty(uint8_t unit_idx, uint8_t duty_pct);

#endif /* FAN_CONTROL_H */
