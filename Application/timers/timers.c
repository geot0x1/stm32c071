#include "timers.h"

/* HAL_TIM_MspPostInit configures timer output GPIO pins (AF mode).
 * Declared in main.h / defined in stm32c0xx_hal_msp.c. */
extern void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* ── Private timer instances ───────────────────────────────────────────────── */

static Tim fan_power_tim;
static Tim fan_remote_tim;
static Tim capture_tim;
static Tim repeater_a_tim;
static Tim repeater_b_tim;
static Tim sys_timer_tim;

/* ── Public API ────────────────────────────────────────────────────────────── */

void timers_init(void)
{
    /* TIM1 — Fan power PWM, 25 kHz, 4 channels */
    tim_pwm_init(&fan_power_tim, TIM1, 25000, 4);
    HAL_TIM_MspPostInit(&fan_power_tim.hal_handle); /* PA2/PA3/PA8/PA9 AF */

    /* TIM3 — Fan remote PWM, 25 kHz, 4 channels */
    tim_pwm_init(&fan_remote_tim, TIM3, 25000, 4);
    HAL_TIM_MspPostInit(&fan_remote_tim.hal_handle); /* PB0/PB1/PB9/PC6 AF */

    /* TIM2 — PWM input capture, 1 MHz resolution */
    tim_ic_init(&capture_tim, TIM2, 1000000);
    HAL_TIM_MspPostInit(&capture_tim.hal_handle); /* PB10/PB11 AF3 + NVIC */

    /* TIM16 — PWM Repeater output A, 1 µs resolution (PSC=47), placeholder ARR */
    tim_pwm_init_raw(&repeater_a_tim, TIM16, 47, 0xFFFF, 1);
    HAL_TIM_MspPostInit(&repeater_a_tim.hal_handle); /* PA0 AF2 */

    /* TIM17 — PWM Repeater output B, 1 µs resolution (PSC=47), placeholder ARR */
    tim_pwm_init_raw(&repeater_b_tim, TIM17, 47, 0xFFFF, 1);
    HAL_TIM_MspPostInit(&repeater_b_tim.hal_handle); /* PA1 AF2 */

    /* TIM14 — System utility timer, 1 MHz free-running base counter */
    tim_base_init(&sys_timer_tim, TIM14, 1000000);
}

/* ── Timer getters ─────────────────────────────────────────────────────────── */

Tim *timers_get_fan_power(void)
{
    return &fan_power_tim;
}

Tim *timers_get_fan_remote(void)
{
    return &fan_remote_tim;
}

Tim *timers_get_capture(void)
{
    return &capture_tim;
}

Tim *timers_get_repeater_a(void)
{
    return &repeater_a_tim;
}

Tim *timers_get_repeater_b(void)
{
    return &repeater_b_tim;
}

Tim *timers_get_sys_timer(void)
{
    return &sys_timer_tim;
}
