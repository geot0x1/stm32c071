#ifndef SYS_TIME_H
#define SYS_TIME_H

#include <stdint.h>

/**
 * @brief  System time tick handler.
 * @note   Must be called from SysTick_Handler every 1ms.
 */
void sys_time_handler(void);

/**
 * @brief  Returns the number of milliseconds since the system started.
 * @retval Number of milliseconds.
 */
uint64_t millis(void);

#endif /* SYS_TIME_H */
