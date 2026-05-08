#include "watchdog.h"
#include "iwdg.h"

#define WATCHDOG_PRESCALER IWDG_PRESCALER_32
#define WATCHDOG_RELOAD 1999U
#define WATCHDOG_WINDOW 4095U

static Iwdg hiwdg;

void watchdog_init(void)
{
    iwdg_init(&hiwdg, WATCHDOG_PRESCALER, WATCHDOG_RELOAD, WATCHDOG_WINDOW);
}

void watchdog_kick(void)
{
    iwdg_refresh(&hiwdg);
}
