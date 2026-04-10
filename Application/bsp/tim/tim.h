#ifndef BSP_TIM_H
#define BSP_TIM_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32c0xx_hal.h"

/**
 * @brief Opaque timer handle
 *
 * Clients allocate a Tim_t struct (typically static) and pass it to
 * tim_*_init(). The handle encapsulates the HAL state and peripheral config.
 */
typedef struct Tim_s {
    // Implementation details — not visible to clients
    TIM_HandleTypeDef hal_handle;
    uint32_t freq_hz;
    uint8_t num_channels;
} Tim_t;

/**
 * @brief Initialize a timer as a PWM output
 *
 * @param tim       Timer handle (allocated by caller)
 * @param instance  TIM peripheral instance (TIM1, TIM3, TIM16, TIM17, etc.)
 * @param freq_hz   PWM frequency in Hz
 * @param num_channels  Number of active channels (1-4)
 *
 * Caller example:
 *   static Tim_t fan_power;
 *   tim_pwm_init(&fan_power, TIM1, 25000, 4);
 */
void tim_pwm_init(Tim_t *tim, TIM_TypeDef *instance, uint32_t freq_hz, uint8_t num_channels);

/**
 * @brief Set PWM duty cycle on a channel (0-100%)
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 * @param duty_pct  Duty cycle (0-100%)
 */
void tim_pwm_set_duty(Tim_t *tim, uint8_t channel, uint8_t duty_pct);

/**
 * @brief Start PWM output on a channel
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 */
void tim_pwm_start(Tim_t *tim, uint8_t channel);

/**
 * @brief Stop PWM output on a channel
 *
 * @param tim       Timer handle
 * @param channel   Channel index (1-4)
 */
void tim_pwm_stop(Tim_t *tim, uint8_t channel);

/**
 * @brief Set PWM frequency (updates ARR/PSC)
 *
 * @param tim       Timer handle
 * @param freq_hz   New frequency in Hz
 */
void tim_pwm_set_freq(Tim_t *tim, uint32_t freq_hz);

/**
 * @brief Initialize a timer for input capture
 *
 * @param tim           Timer handle
 * @param instance      TIM peripheral instance (TIM2, etc.)
 * @param resolution_hz Counting frequency (e.g., 1000000 for 1µs resolution)
 */
void tim_ic_init(Tim_t *tim, TIM_TypeDef *instance, uint32_t resolution_hz);

/**
 * @brief Get the current counter value
 *
 * @param tim       Timer handle
 * @return          Counter value
 */
uint32_t tim_ic_get_count(Tim_t *tim);

/**
 * @brief Get capture value on a channel
 *
 * @param tim       Timer handle
 * @param channel   Channel index
 * @return          Captured value
 */
uint32_t tim_ic_get_channel(Tim_t *tim, uint8_t channel);

/**
 * @brief Initialize a timer as a free-running base counter
 *
 * @param tim       Timer handle
 * @param instance  TIM peripheral instance (TIM14, etc.)
 * @param tick_hz   Counting frequency in Hz
 */
void tim_base_init(Tim_t *tim, TIM_TypeDef *instance, uint32_t tick_hz);

/**
 * @brief Get the current counter value
 *
 * @param tim       Timer handle
 * @return          Counter value
 */
uint32_t tim_base_get_count(Tim_t *tim);

/**
 * @brief Enable timer interrupt
 *
 * @param tim       Timer handle
 */
void tim_enable_irq(Tim_t *tim);

/**
 * @brief Disable timer interrupt
 *
 * @param tim       Timer handle
 */
void tim_disable_irq(Tim_t *tim);

#endif // BSP_TIM_H
