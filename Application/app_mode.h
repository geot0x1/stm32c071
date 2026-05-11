#ifndef APP_MODE_H
#define APP_MODE_H

typedef enum
{
    ModeNormal,
    ModeManual,
} AppMode;

typedef enum
{
    SystemLow,
    SystemHigh,
    SystemThrottling,
    SystemCritical,
    SystemSensorLost,
    SystemError,
} SystemState;

void app_set_mode(AppMode mode);
AppMode app_get_mode(void);
void app_set_state(SystemState state);
SystemState app_get_state(void);

#endif /* APP_MODE_H */
