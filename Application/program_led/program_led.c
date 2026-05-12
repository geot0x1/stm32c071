#include "program_led.h"
#include "board.h"
#include "stm32c0xx_hal.h"
#include <stddef.h>

typedef struct
{
    uint32_t duration_ms;
    bool     is_on;
} PatternStep;

typedef struct
{
    const PatternStep *steps;
    size_t             step_count;
} BlinkPattern;

static const PatternStep low_pattern[] = {
    {300U, true},
    {2000U, false},
};

static const PatternStep high_pattern[] = {
    {100U, true},
    {100U, false},
    {100U, true},
    {800U, false},
};

static const PatternStep throttling_pattern[] = {
    {100U, true},
    {100U, false},
    {100U, true},
    {100U, false},
    {100U, true},
    {1000U, false},
};

static const PatternStep critical_pattern[] = {
    {100U, true},
    {100U, false},
};

static const PatternStep error_pattern[] = {
    {100U, true},
    {100U, false},
};

static const BlinkPattern patterns[] = {
    {low_pattern, sizeof(low_pattern) / sizeof(low_pattern[0])},
    {high_pattern, sizeof(high_pattern) / sizeof(high_pattern[0])},
    {throttling_pattern, sizeof(throttling_pattern) / sizeof(throttling_pattern[0])},
    {critical_pattern, sizeof(critical_pattern) / sizeof(critical_pattern[0])},
    {error_pattern, sizeof(error_pattern) / sizeof(error_pattern[0])},
};

static ProgramLedState current_state;
static size_t pattern_step;
static uint32_t last_tick;

void program_led_init(void)
{
    current_state = ProgramLedLow;
    pattern_step = 0U;
    last_tick = HAL_GetTick();
    board_led_set(patterns[current_state].steps[0].is_on);
}

void program_led_set_state(ProgramLedState state)
{
    if (current_state == state)
    {
        return;
    }
    current_state = state;
    pattern_step = 0U;
    last_tick = HAL_GetTick();
    board_led_set(patterns[current_state].steps[0].is_on);
}

void program_led_task(void)
{
    const BlinkPattern *pattern = &patterns[current_state];
    const PatternStep *step = &pattern->steps[pattern_step];

    uint32_t elapsed = HAL_GetTick() - last_tick;
    if (elapsed >= step->duration_ms)
    {
        pattern_step = (pattern_step + 1U) % pattern->step_count;
        last_tick = HAL_GetTick();
        board_led_set(pattern->steps[pattern_step].is_on);
    }
}
