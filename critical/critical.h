#ifndef CRITICAL_H
#define CRITICAL_H

#include <stdint.h>

/**
 * @brief  Enter critical section (disable global interrupts)
 * @note   Nested calls are supported.
 */
void critical_enter(void);

/**
 * @brief  Exit critical section (re-enable global interrupts if nesting count is zero)
 */
void critical_exit(void);

#endif /* CRITICAL_H */
