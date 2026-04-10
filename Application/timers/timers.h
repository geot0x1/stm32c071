#ifndef TIMERS_H
#define TIMERS_H

#include "tim.h"

/**
 * @brief Initialize all timer peripherals.
 *
 * Call once at startup after board_init().
 * Configures GPIO alternate functions, BSP timer init, and NVIC for each timer.
 */
void timers_init(void);

/* ── Timer getters ───────────────────────────────────────────────────────── */

/** @brief TIM1 — Fan power PWM, 25 kHz, 4 channels → fan_control */
Tim_t *timers_get_fan_power(void);

/** @brief TIM3 — Fan remote PWM, 25 kHz, 4 channels → fan_control */
Tim_t *timers_get_fan_remote(void);

/** @brief TIM2 — PWM input capture, 1 MHz resolution → pwm_repeater */
Tim_t *timers_get_capture(void);

/** @brief TIM16 — PWM repeater output A, 25 kHz, 1 channel → pwm_repeater */
Tim_t *timers_get_repeater_a(void);

/** @brief TIM17 — PWM repeater output B, 25 kHz, 1 channel → pwm_repeater */
Tim_t *timers_get_repeater_b(void);

#endif /* TIMERS_H */
