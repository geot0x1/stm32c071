#include "push_button.h"
#include "board.h"
#include "stm32c0xx_hal.h"

#define DEBOUNCE_MS 20U

typedef struct
{
    bool stableState;
    bool currentLevel;
    uint32_t debounceStart;
} PushButtonState;

static PushButtonState button_state;

static bool debounce_timer_expired(void)
{
    return (HAL_GetTick() - button_state.debounceStart) >= DEBOUNCE_MS;
}

void push_button_init(void)
{
    button_state.stableState = board_fan_force_en_read();
    button_state.currentLevel = button_state.stableState;
    button_state.debounceStart = HAL_GetTick();
}

void push_button_task(void)
{
    bool raw = board_fan_force_en_read();

    if (raw != button_state.currentLevel)
    {
        button_state.currentLevel = raw;
        button_state.debounceStart = HAL_GetTick();
    }
    else if (debounce_timer_expired())
    {
        button_state.stableState = button_state.currentLevel;
    }
}

bool push_button_is_pressed(void)
{
    return button_state.stableState;
}
