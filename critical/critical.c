#include "critical.h"
#include "stm32c0xx_hal.h"

/**
 * @brief  Nesting counter for critical sections.
 */
static uint32_t _nesting_count = 0;

void critical_enter(void)
{
    __disable_irq();
    _nesting_count++;
}

void critical_exit(void)
{
    if (_nesting_count > 0)
    {
        _nesting_count--;
        if (_nesting_count == 0)
        {
            __enable_irq();
        }
    }
}
