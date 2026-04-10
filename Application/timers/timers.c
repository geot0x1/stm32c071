#include "timers.h"

/* HAL_TIM_MspPostInit configures timer output GPIO pins (AF mode).
 * Declared in main.h / defined in stm32c0xx_hal_msp.c. */
extern void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* ── Private timer instances ───────────────────────────────────────────────── */

static Tim_t fan_power_tim;
static Tim_t fan_remote_tim;
static Tim_t capture_tim;
static Tim_t repeater_a_tim;
static Tim_t repeater_b_tim;

/* ── Public API ────────────────────────────────────────────────────────────── */

void timers_init(void)
{
    /* TIM1 — Fan power PWM, 25 kHz, 4 channels */
    tim_pwm_init(&fan_power_tim, TIM1, 25000, 4);
    HAL_TIM_MspPostInit(&fan_power_tim.hal_handle);   /* PA2/PA3/PA8/PA9 AF */

    /* TIM3 — Fan remote PWM, 25 kHz, 4 channels */
    tim_pwm_init(&fan_remote_tim, TIM3, 25000, 4);
    HAL_TIM_MspPostInit(&fan_remote_tim.hal_handle);  /* PB0/PB1/PB9/PC6 AF */

    /* TIM2 — PWM input capture, 1 MHz resolution */
    tim_ic_init(&capture_tim, TIM2, 1000000);
    /* PB10/PB11 AF3 (TIM2_CH3/CH4) + NVIC — not in MspPostInit, set here */
    {
        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = GPIO_PIN_10 | GPIO_PIN_11;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_LOW;
        gpio.Alternate = GPIO_AF3_TIM2;
        HAL_GPIO_Init(GPIOB, &gpio);
    }
    HAL_NVIC_SetPriority(TIM2_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    /* TIM16 — PWM Repeater output A, 25 kHz, 1 channel */
    tim_pwm_init(&repeater_a_tim, TIM16, 25000, 1);
    HAL_TIM_MspPostInit(&repeater_a_tim.hal_handle);  /* PA0 AF2 */

    /* TIM17 — PWM Repeater output B, 25 kHz, 1 channel */
    tim_pwm_init(&repeater_b_tim, TIM17, 25000, 1);
    HAL_TIM_MspPostInit(&repeater_b_tim.hal_handle);  /* PA1 AF2 */
}

/* ── Timer getters ─────────────────────────────────────────────────────────── */

Tim_t *timers_get_fan_power(void)
{
    return &fan_power_tim;
}

Tim_t *timers_get_fan_remote(void)
{
    return &fan_remote_tim;
}

Tim_t *timers_get_capture(void)
{
    return &capture_tim;
}

Tim_t *timers_get_repeater_a(void)
{
    return &repeater_a_tim;
}

Tim_t *timers_get_repeater_b(void)
{
    return &repeater_b_tim;
}
