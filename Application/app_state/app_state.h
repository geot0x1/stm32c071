#ifndef APP_STATE_H
#define APP_STATE_H

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

void app_state_init(void);
void app_set_mode(AppMode mode);
AppMode app_get_mode(void);
void app_set_state(SystemState state);
SystemState app_get_state(void);

#endif /* APP_STATE_H */
