#include "fan_control.h"
#include "stm32c0xx_hal.h"

extern TIM_HandleTypeDef htim1;

static uint8_t _fan_duties[4] = {0, 0, 0, 0};

void fan_control_init(uint32_t frequency_hz)
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
        // Frequency too high for 48MHz clock (max ~48MHz, but let's be safe)
        return;
    }

    htim1.Instance->PSC = psc;
    htim1.Instance->ARR = arr;
    htim1.Instance->EGR = TIM_EGR_UG; // Generate an update event to apply PSC/ARR

    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
}

void fan_control_set_duty(FanChannel channel, uint8_t duty_pct)
{
    if (duty_pct > 100)
    {
        duty_pct = 100;
    }

    if (channel < FanChannel1 || channel > FanChannel4)
    {
        return;
    }

    _fan_duties[channel - 1] = duty_pct;

    uint32_t arr = htim1.Instance->ARR;
    uint32_t pulse = (uint32_t)(((uint64_t)duty_pct * (arr + 1)) / 100);

    uint32_t timChannel = 0;
    switch (channel)
    {
        case FanChannel1:
            timChannel = TIM_CHANNEL_1;
            break;
        case FanChannel2:
            timChannel = TIM_CHANNEL_2;
            break;
        case FanChannel3:
            timChannel = TIM_CHANNEL_3;
            break;
        case FanChannel4:
            timChannel = TIM_CHANNEL_4;
            break;
        default:
            return;
    }

    __HAL_TIM_SET_COMPARE(&htim1, timChannel, pulse);
}

uint8_t fan_control_get_duty(FanChannel channel)
{
    if (channel < FanChannel1 || channel > FanChannel4)
    {
        return 0;
    }
    return _fan_duties[channel - 1];
}
