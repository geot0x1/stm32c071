#ifndef DELAY_H
#define DELAY_H

#include <stdint.h>

/* Forward declaration — Tim is defined in tim.h */
typedef struct TimS Tim;

/**
 * @brief  Registers the pre-initialised system timer handle for use by delay functions.
 *         TIM14 must already be running at 1 MHz (done by timers_init()).
 */
void delay_init(Tim *tim);

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
