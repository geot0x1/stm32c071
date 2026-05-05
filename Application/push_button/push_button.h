#ifndef PUSH_BUTTON_H
#define PUSH_BUTTON_H

#include <stdbool.h>

void push_button_init(void);
void push_button_task(void);
bool push_button_is_pressed(void);

#endif /* PUSH_BUTTON_H */
