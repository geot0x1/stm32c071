#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

/**
 * @brief  Initializes TIM14 for 1us per tick delay.
 */
void delay_init(void);

/**
 * @brief  Blocking delay in microseconds.
 * @param  us: Number of microseconds to wait.
 */
void delay_us(uint32_t us);

/**
 * @brief  Blocking delay in milliseconds.
 * @param  ms: Number of milliseconds to wait.
 */
void delay_ms(uint32_t ms);

#endif /* DELAY_H */
