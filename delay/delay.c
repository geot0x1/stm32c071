#include "delay.h"
#include "stm32c0xx_hal.h"

void delay_init(void)
{
    __HAL_RCC_TIM14_CLK_ENABLE();

    TIM14->PSC = (SystemCoreClock / 1000000) - 1;
    TIM14->ARR = 0xFFFF;
    TIM14->EGR |= TIM_EGR_UG;
    TIM14->CR1 |= TIM_CR1_CEN;
}

void delay_us(uint32_t us)
{
    // Safety check to prevent infinite loop if TIM14 is not enabled
    if (!(TIM14->CR1 & TIM_CR1_CEN) || (us == 0))
    {
        return;
    }

    uint16_t start = (uint16_t)TIM14->CNT;
    while ((uint16_t)(TIM14->CNT - start) < (uint16_t)us)
    {
        __NOP();
    }
}

void delay_ms(uint32_t ms)
{
    while (ms--)
    {
        delay_us(1000);
    }
}
