#ifndef APP_STATE_H
#define APP_STATE_H

typedef enum
{
    SystemBoot,
    SystemRunning,
    SystemFault,
} SystemState;

typedef enum
{
    ThermalLow,
    ThermalHigh,
    ThermalThrottling,
    ThermalCritical,
} ThermalState;

void         app_state_init(void);
SystemState  app_get_state(void);
ThermalState app_get_thermal_state(void);

void app_state_enter_running(ThermalState initial);
void app_state_enter_fault(void);
void app_state_update_thermal(ThermalState s);

#endif /* APP_STATE_H */
