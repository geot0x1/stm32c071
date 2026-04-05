#include "fan_control.h"
#include "stm32c0xx_hal.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;

static uint8_t _power_duties[4] = {0, 0, 0, 0};
static uint8_t _remote_duties[4] = {0, 0, 0, 0};

typedef struct
{
    FanChannel tim1_pwr_channel;   /**< Power channel on TIM1 */
    FanChannel tim3_ctrl_channel;  /**< Remote/Control channel on TIM3 */
} FanLink;

static const FanLink _fan_links[4] = {
    {FAN_CHANNEL2, FAN_CHANNEL1}, // Unit 1: Power (TIM1_CH2) + Remote (TIM3_CH1)
    {FAN_CHANNEL1, FAN_CHANNEL4}, // Unit 2: Power (TIM1_CH1) + Remote (TIM3_CH4)
    {FAN_CHANNEL4, FAN_CHANNEL3}, // Unit 3: Power (TIM1_CH4) + Remote (TIM3_CH3)
    {FAN_CHANNEL3, FAN_CHANNEL2}  // Unit 4: Power (TIM1_CH3) + Remote (TIM3_CH2)
};

static uint32_t get_hal_channel(FanChannel channel)
{
    switch (channel)
    {
        case FAN_CHANNEL1:
            return TIM_CHANNEL_1;
        case FAN_CHANNEL2:
            return TIM_CHANNEL_2;
        case FAN_CHANNEL3:
            return TIM_CHANNEL_3;
        case FAN_CHANNEL4:
            return TIM_CHANNEL_4;
        default:
            return 0;
    }
}

static void set_timer_freq(TIM_HandleTypeDef *htim, uint32_t frequency_hz)
{
    if (frequency_hz == 0)
    {
        return;
    }

    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t totalTicks = pclk / frequency_hz;
    uint32_t psc = 0;
    uint32_t arr = 0;

    if (totalTicks > 65536)
    {
        psc = totalTicks / 65536;
        arr = (totalTicks / (psc + 1)) - 1;
    }
    else if (totalTicks > 0)
    {
        psc = 0;
        arr = totalTicks - 1;
    }
    else
    {
        return;
    }

    htim->Instance->PSC = psc;
    htim->Instance->ARR = arr;
    htim->Instance->EGR = TIM_EGR_UG;
}

void fan_power_init(uint32_t frequency_hz)
{
    set_timer_freq(&htim1, frequency_hz);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
}

void fan_remote_init(uint32_t frequency_hz)
{
    set_timer_freq(&htim3, frequency_hz);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);
}

void fan_init(uint32_t frequency_hz)
{
    fan_power_init(frequency_hz);
    fan_remote_init(frequency_hz);
}

void fan_control_init(void)
{
    // Links are now defined at compile-time
}

void fan_control_set_power_channel_duty(FanChannel channel, uint8_t duty_pct)
{
    if (channel < FAN_CHANNEL1 || channel > FAN_CHANNEL4)
    {
        return;
    }
    if (duty_pct > 100)
    {
        duty_pct = 100;
    }

    _power_duties[channel - 1] = duty_pct;
    uint32_t arr = htim1.Instance->ARR;
    uint32_t pulse = (uint32_t)(((uint64_t)duty_pct * (arr + 1)) / 100);
    __HAL_TIM_SET_COMPARE(&htim1, get_hal_channel(channel), pulse);
}

void fan_control_set_remote_channel_duty(FanChannel channel, uint8_t duty_pct)
{
    if (channel < FAN_CHANNEL1 || channel > FAN_CHANNEL4)
    {
        return;
    }
    if (duty_pct > 100)
    {
        duty_pct = 100;
    }

    _remote_duties[channel - 1] = duty_pct;
    uint32_t arr = htim3.Instance->ARR;
    uint32_t pulse = (uint32_t)(((uint64_t)duty_pct * (arr + 1)) / 100);
    __HAL_TIM_SET_COMPARE(&htim3, get_hal_channel(channel), pulse);
}

uint8_t fan_control_get_power_channel_duty(FanChannel channel)
{
    if (channel < FAN_CHANNEL1 || channel > FAN_CHANNEL4)
    {
        return 0;
    }
    return _power_duties[channel - 1];
}

uint8_t fan_control_get_remote_channel_duty(FanChannel channel)
{
    if (channel < FAN_CHANNEL1 || channel > FAN_CHANNEL4)
    {
        return 0;
    }
    return _remote_duties[channel - 1];
}

void fan_control_set_unit_duty(uint8_t unit_idx, uint8_t duty_pct)
{
    if (unit_idx < 1 || unit_idx > 4)
    {
        return;
    }
    uint8_t idx = unit_idx - 1;
    fan_control_set_power_channel_duty(_fan_links[idx].tim1_pwr_channel, duty_pct);
    fan_control_set_remote_channel_duty(_fan_links[idx].tim3_ctrl_channel, duty_pct);
}
