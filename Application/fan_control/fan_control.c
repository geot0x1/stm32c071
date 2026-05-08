#include "fan_control.h"
#include "gpio.h"
#include "board_config.h"
#include "tim.h"

static Tim *_power_tim = NULL;
static Tim *_remote_tim = NULL;

static FanType _fan_types[4];

typedef struct
{
    FanChannel tim1_pwr_channel; /**< Power channel on TIM1 */
    FanChannel tim3_ctrl_channel; /**< Control channel on TIM3 */
} FanLink;

static const FanLink _fan_links[4] = {
    {FanChannelTwo, FanChannelOne}, /* Unit 1: TIM1_CH2 (PA9) + TIM3_CH1 (PC6) */
    {FanChannelOne, FanChannelFour}, /* Unit 2: TIM1_CH1 (PA8) + TIM3_CH4 (PB1) */
    {FanChannelFour, FanChannelThree}, /* Unit 3: TIM1_CH4 (PA3) + TIM3_CH3 (PB0) */
    {FanChannelThree, FanChannelTwo}, /* Unit 4: TIM1_CH3 (PA2) + TIM3_CH2 (PB9) */
};

static const uint16_t _type_pins[4] = {
    BOARD_FAN1_TYPE_PIN,
    BOARD_FAN2_TYPE_PIN,
    BOARD_FAN3_TYPE_PIN,
    BOARD_FAN4_TYPE_PIN,
};

void fan_control_init(Tim *power_tim, Tim *remote_tim)
{
    _power_tim = power_tim;
    _remote_tim = remote_tim;

    for (uint8_t i = 0; i < 4; i++)
    {
        _fan_types[i] = FanType2Wire;  // Force all fans to 2-wire mode
    }
}

void fan_power_init(uint32_t frequency_hz)
{
    tim_pwm_set_freq(_power_tim, frequency_hz);
    tim_pwm_start(_power_tim, FanChannelOne);
    tim_pwm_start(_power_tim, FanChannelTwo);
    tim_pwm_start(_power_tim, FanChannelThree);
    tim_pwm_start(_power_tim, FanChannelFour);
}

void fan_remote_init(uint32_t frequency_hz)
{
    tim_pwm_set_freq(_remote_tim, frequency_hz);
    tim_pwm_start(_remote_tim, FanChannelOne);
    tim_pwm_start(_remote_tim, FanChannelTwo);
    tim_pwm_start(_remote_tim, FanChannelThree);
    tim_pwm_start(_remote_tim, FanChannelFour);
}

void fan_init(uint32_t frequency_hz)
{
    fan_power_init(frequency_hz);
    fan_remote_init(frequency_hz);
}

void fan_control_set_power_channel_duty(FanChannel channel, uint8_t duty_pct)
{
    if (channel < FanChannelOne || channel > FanChannelFour)
    {
        return;
    }
    if (duty_pct > 100)
    {
        duty_pct = 100;
    }
    tim_pwm_set_duty(_power_tim, (uint8_t)channel, duty_pct);
}

void fan_control_set_remote_channel_duty(FanChannel channel, uint8_t duty_pct)
{
    if (channel < FanChannelOne || channel > FanChannelFour)
    {
        return;
    }
    if (duty_pct > 100)
    {
        duty_pct = 100;
    }
    uint8_t inverted = 100 - duty_pct;
    tim_pwm_set_duty(_remote_tim, (uint8_t)channel, inverted);
}

uint8_t fan_control_get_power_channel_duty(FanChannel channel)
{
    if (channel < FanChannelOne || channel > FanChannelFour)
    {
        return 0;
    }
    return tim_pwm_get_duty(_power_tim, (uint8_t)channel);
}

uint8_t fan_control_get_remote_channel_duty(FanChannel channel)
{
    if (channel < FanChannelOne || channel > FanChannelFour)
    {
        return 0;
    }
    uint8_t raw = tim_pwm_get_duty(_remote_tim, (uint8_t)channel);
    return 100 - raw;
}

void fan_control_set_unit_duty(uint8_t unit_idx, uint8_t duty_pct)
{
    if (unit_idx < 1 || unit_idx > 4)
    {
        return;
    }
    uint8_t idx = unit_idx - 1;
    uint8_t on_off = (duty_pct > 0) ? 100 : 0;

    FanChannel pwr_ch = _fan_links[idx].tim1_pwr_channel;
    FanChannel ctrl_ch = _fan_links[idx].tim3_ctrl_channel;

    if (_fan_types[idx] == FanType2Wire)
    {
        fan_control_set_power_channel_duty(pwr_ch, on_off);
    }
    else
    {
        fan_control_set_power_channel_duty(pwr_ch, on_off);
        fan_control_set_remote_channel_duty(ctrl_ch, on_off);
    }
}

uint8_t fan_control_get_unit_duty(uint8_t unit_idx)
{
    if (unit_idx < 1 || unit_idx > 4)
    {
        return 0;
    }
    return fan_control_get_power_channel_duty(_fan_links[unit_idx - 1].tim1_pwr_channel);
}

FanType fan_control_get_type(uint8_t unit_idx)
{
    if (unit_idx < 1 || unit_idx > 4)
    {
        return FanType2Wire;
    }
    return _fan_types[unit_idx - 1];
}
