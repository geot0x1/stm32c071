#include "push_button.h"
#include "board.h"
#include "stm32c0xx_hal.h"

#define DEBOUNCE_MS 20U

static bool stableState;
static bool currentLevel;
static uint32_t debounceStart;

void push_button_init(void)
{
    stableState = board_fan_force_en_read();
    currentLevel = stableState;
    debounceStart = HAL_GetTick();
}

void push_button_task(void)
{
    bool raw = board_fan_force_en_read();

    if (raw != currentLevel)
    {
        currentLevel = raw;
        debounceStart = HAL_GetTick();
    }
    else if (HAL_GetTick() - debounceStart >= DEBOUNCE_MS)
    {
        stableState = currentLevel;
    }
}

bool push_button_is_pressed(void)
{
    return stableState;
}
