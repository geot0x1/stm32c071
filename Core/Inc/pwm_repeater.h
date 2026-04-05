/**
 * @file pwm_repeater.h
 * @brief Public API for the PWM Repeater/Capper module on TIM2.
 * Target: STM32C071, TIM2 (32-bit).
 */

#ifndef PWM_REPEATER_H
#define PWM_REPEATER_H

#include "stm32c0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Structure holding state for one PWM Repeater pair.
 * All fields shared with ISRs are marked volatile.
 */
typedef struct
{
    volatile uint32_t rise_timestamp;    /* Raw CCR value at last rising edge */
    volatile uint32_t fall_timestamp;    /* Raw CCR value at last falling edge */
    volatile uint32_t period_ticks;      /* Rise-to-rise delta (32-bit wrapping safe) */
    volatile uint32_t previous_period_ticks; /* Previous period ticks for averaging */
    volatile uint32_t pulse_ticks;       /* Rise-to-fall delta */
    volatile uint32_t low_level_ticks;   /* Fall-to-rise delta */
    volatile bool rise_captured;         /* True after first rising edge seen */
    volatile bool fall_captured;         /* True after first falling edge seen */
    volatile bool new_data_ready;        /* Set when a new duty is computed */
    volatile uint32_t period_stable_counter; /* Counter for stable periods (debounce) */
    volatile uint32_t pulse_stable_counter;  /* Counter for stable pulses (debounce) */
    volatile uint32_t previous_pulse_ticks;  /* Previous pulse ticks for stability check */
    volatile uint32_t last_capture_ms;   /* HAL Tick at last valid capture for watchdog */
} PWM_Channel_t;

/**
 * @brief Throttle Modes:
 * - Scale: Output = Input * (Throttle / 100)
 * - Fixed: Output = Throttle (capped by Input)
 */
typedef enum
{
    ThrottleMode_Scale,
    ThrottleMode_Fixed
} ThrottleMode;

/**
 * @brief Structure holding output control data for one PWM channel.
 */
typedef struct
{
    TIM_HandleTypeDef *htim;             /* Timer peripheral handle */
    uint32_t channel;                    /* TIM_CHANNEL_x */
    volatile uint32_t period_ticks;      /* Target ARR value */
    volatile uint32_t pulse_ticks;       /* Target CCR value */
    volatile uint32_t cap_factor_pct;    /* 0-100: Safety limit */
    volatile uint32_t throttle_val;      /* 0-100: Scale or Fixed value */
    volatile ThrottleMode throttle_mode; /* Mode for throttling */
} PWM_Output_t;

/* Global channel instances */
extern PWM_Channel_t pwmChA;
extern PWM_Channel_t pwmChB;
extern PWM_Output_t pwmOutA;
extern PWM_Output_t pwmOutB;

/**
 * @brief Initializes TIM2 channels and state variables for PWM repeating.
 */
void pwm_repeater_init(void);
void pwm_repeater_task(void);

void pwm_set_throttle_a(uint32_t val, ThrottleMode mode);
void pwm_set_throttle_b(uint32_t val, ThrottleMode mode);

uint32_t pwm_get_frequency_a(void);
uint32_t pwm_get_duty_a(void);
uint32_t pwm_get_frequency_b(void);
uint32_t pwm_get_duty_b(void);

uint32_t get_ticks(void);

#endif /* PWM_REPEATER_H */
