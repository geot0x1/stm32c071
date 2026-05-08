#ifndef PROGRAM_LED_H
#define PROGRAM_LED_H

typedef enum
{
    ProgramLedFansOff, /* ON 100ms / OFF 2000ms */
    ProgramLedFansOn,  /* ON 100ms / OFF 1000ms */
    ProgramLedError,   /* ON 200ms / OFF 200ms  */
} ProgramLedState;

void program_led_init(void);
void program_led_set_state(ProgramLedState state);
void program_led_task(void);

#endif /* PROGRAM_LED_H */
