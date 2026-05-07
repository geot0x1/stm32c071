#include "push_button.h"
#include "board.h"
#include "stm32c0xx_hal.h"

#define DEBOUNCE_MS 20U

typedef enum
{
    DebounceIdle,
    DebounceWaiting,
} DebouncePhase;

static bool stableState;
static bool pendingState;
static DebouncePhase phase;
static uint32_t debounceStart;

void push_button_init(void)
{
    stableState = board_fan_force_en_read();
    pendingState = stableState;
    phase = DebounceIdle;
    debounceStart = 0U;
}

void push_button_task(void)
{
    bool raw = board_fan_force_en_read();

    switch (phase)
    {
        case DebounceIdle:
            if (raw != stableState)
            {
                pendingState = raw;
                debounceStart = HAL_GetTick();
                phase = DebounceWaiting;
            }
            break;

        case DebounceWaiting:
            if (raw != pendingState)
            {
                phase = DebounceIdle;
            }
            else if (HAL_GetTick() - debounceStart >= DEBOUNCE_MS)
            {
                stableState = pendingState;
                phase = DebounceIdle;
            }
            break;
    }
}

bool push_button_is_pressed(void)
{
    return stableState;
}
