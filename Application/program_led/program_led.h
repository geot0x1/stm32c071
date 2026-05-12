#ifndef PROGRAM_LED_H
#define PROGRAM_LED_H

typedef enum
{
    ProgramLedLow,        /* Slow blink: ON 300ms / OFF 2000ms */
    ProgramLedHigh,       /* 2 fast blinks + long off: ON 100 / OFF 100 / ON 100 / OFF 800ms */
    ProgramLedThrottling, /* 3 fast blinks + long off: ON 100 / OFF 100 / ON 100 / OFF 100 / ON 100 / OFF 1000ms */
    ProgramLedCritical,   /* Fast blink: ON 100ms / OFF 100ms */
    ProgramLedError,      /* Fast blink: ON 100ms / OFF 100ms */
} ProgramLedState;

void program_led_init(void);
void program_led_set_state(ProgramLedState state);
void program_led_task(void);

#endif /* PROGRAM_LED_H */
