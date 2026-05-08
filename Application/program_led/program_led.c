#include "program_led.h"
#include "board.h"
#include "stm32c0xx_hal.h"

static const uint32_t ON_MS[]  = {100U, 100U, 200U};
static const uint32_t OFF_MS[] = {2000U, 1000U, 200U};

static ProgramLedState current_state;
static uint32_t last_tick;
static bool led_on;

void program_led_init(void)
{
    current_state = ProgramLedFansOff;
    last_tick = HAL_GetTick();
    led_on = false;
    board_led_set(false);
}

void program_led_set_state(ProgramLedState state)
{
    if (current_state == state)
    {
        return;
    }
    current_state = state;
    last_tick = HAL_GetTick();
    led_on = false;
    board_led_set(false);
}

void program_led_task(void)
{
    uint32_t elapsed = HAL_GetTick() - last_tick;
    uint32_t threshold = led_on ? ON_MS[current_state] : OFF_MS[current_state];

    if (elapsed >= threshold)
    {
        last_tick = HAL_GetTick();
        led_on = !led_on;
        board_led_set(led_on);
    }
}
