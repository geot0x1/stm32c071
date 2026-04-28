/**
 * @file pwm_repeater.h
 * @brief Public API for the PWM Repeater/Capper module.
 * Target: STM32C071, TIM2 (32-bit input capture) + TIM16/TIM17 (PWM output).
 */

#ifndef PWM_REPEATER_H
#define PWM_REPEATER_H

#include <stdbool.h>
#include <stdint.h>
#include "tim.h"

/**
 * @brief State for one PWM input channel.
 * Fields shared with ISRs are marked volatile.
 */
typedef struct
{
    volatile uint32_t rise_timestamp;
    volatile uint32_t fall_timestamp;
    volatile uint32_t period_ticks;
    volatile uint32_t previous_period_ticks;
    volatile uint32_t pulse_ticks;
    volatile uint32_t low_level_ticks;
    volatile bool     rise_captured;
    volatile bool     fall_captured;
    volatile bool     new_data_ready;
    volatile uint32_t period_stable_counter;
    volatile uint32_t pulse_stable_counter;
    volatile uint32_t previous_pulse_ticks;
    volatile uint32_t last_capture_ms;
} PwmChannel;

/**
 * @brief Output control state for one PWM channel.
 */
typedef struct
{
    Tim            *tim;               /* BSP timer handle (TIM16 or TIM17) */
    uint32_t          channel;           /* TIM_CHANNEL_x */
    volatile uint32_t period_ticks;
    volatile uint32_t pulse_ticks;     /* actual input HIGH time (post-BJT correction) */
    volatile uint32_t throttle_val;    /* limit in percent, 0–100 */
} PwmOutput;

/* Global channel instances (read-only from outside this module) */
extern PwmChannel pwmChannelA;
extern PwmChannel pwmChannelB;
extern PwmOutput  pwmOutputA;
extern PwmOutput  pwmOutputB;

/**
 * @brief Initialize the PWM repeater with BSP timer handles.
 *
 * @param capture_tim  TIM2 input capture handle (board_get_capture_tim())
 * @param out_a_tim    TIM16 PWM output handle   (board_get_repeater_a_tim())
 * @param out_b_tim    TIM17 PWM output handle   (board_get_repeater_b_tim())
 */
void pwm_repeater_init(Tim *capture_tim, Tim *out_a_tim, Tim *out_b_tim);

/** @brief Periodic watchdog / output update task. Call from main loop. */
void pwm_repeater_task(void);

/** @brief Route TIM2 global IRQ here from stm32c0xx_it.c. */
void pwm_repeater_tim2_irq_handler(void);

void pwm_set_throttle_a(uint32_t limit_pct);
void pwm_set_throttle_b(uint32_t limit_pct);

uint32_t pwm_get_frequency_a(void);
uint32_t pwm_get_duty_a(void);
uint32_t pwm_get_frequency_b(void);
uint32_t pwm_get_duty_b(void);

#endif /* PWM_REPEATER_H */
