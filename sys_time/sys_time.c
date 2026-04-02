#include "sys_time.h"
#include "main.h"

/**
 * @brief  Global system tick counter.
 * @note   Updated every millisecond.
 */
static volatile uint64_t systemTick = 0;

void sys_time_handler(void)
{
    systemTick++;
}

uint64_t millis(void)
{
    uint64_t m;
    uint32_t pri = __get_PRIMASK();
    __disable_irq();
    m = systemTick;
    if (!pri)
    {
        __enable_irq();
    }
    return m;
}
