#include "push_button.h"
#include "board.h"
#include "sys_time.h"

#define DEBOUNCE_MS 20U

typedef struct
{
    bool stableState;
    bool currentLevel;
    millis_t debounceStart;
} PushButtonState;

static PushButtonState button_state;

static bool debounce_timer_expired(void)
{
    return (millis() - button_state.debounceStart) >= DEBOUNCE_MS;
}

void push_button_init(void)
{
    button_state.stableState = board_fan_force_en_read();
    button_state.currentLevel = button_state.stableState;
    button_state.debounceStart = millis();
}

void push_button_task(void)
{
    bool raw = board_fan_force_en_read();

    if (raw != button_state.currentLevel)
    {
        button_state.currentLevel = raw;
        button_state.debounceStart = millis();
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
