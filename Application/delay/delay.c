#include "delay.h"
#include "tim.h"
#include <stddef.h>

static Tim *tim_handle = NULL;

void delay_init(Tim *tim)
{
    tim_handle = tim;
}

void delay_us(uint32_t us)
{
    if (tim_handle == NULL || us == 0)
    {
        return;
    }

    while (us > 0)
    {
        uint16_t chunk = (us >= 65535U) ? 65535U : (uint16_t)us;
        uint16_t start = (uint16_t)tim_base_get_count(tim_handle);
        while ((uint16_t)(tim_base_get_count(tim_handle) - start) < chunk)
        {
            __NOP();
        }
        us -= chunk;
    }
}

void delay_ms(uint32_t ms)
{
    while (ms--)
    {
        delay_us(1000);
    }
}
