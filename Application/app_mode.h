#ifndef APP_MODE_H
#define APP_MODE_H

typedef enum
{
    ModeNormal,
    ModeManual,
} AppMode;

void app_set_mode(AppMode mode);
AppMode app_get_mode(void);

#endif /* APP_MODE_H */
