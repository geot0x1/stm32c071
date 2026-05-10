#include "sys_time.h"
#include "stm32c0xx_hal.h"

/**
 * @brief  Global system tick counter.
 * @note   Updated every millisecond.
 */
static volatile uint64_t systemTick = 0;

void sys_time_handler(void)
{
    systemTick++;
}

millis_t millis(void)
{
    millis_t m;
    uint32_t pri = __get_PRIMASK();
    __disable_irq();
    m = systemTick;
    if (!pri)
    {
        __enable_irq();
    }
    return m;
}
