#include "app_state.h"

static SystemState  system_state  = SystemBoot;
static ThermalState thermal_state = ThermalLow;

void app_state_init(void)
{
    system_state  = SystemBoot;
    thermal_state = ThermalLow;
}

SystemState app_get_state(void)
{
    return system_state;
}

ThermalState app_get_thermal_state(void)
{
    return thermal_state;
}

void app_state_enter_running(ThermalState initial)
{
    if (system_state == SystemBoot || system_state == SystemFault)
    {
        system_state  = SystemRunning;
        thermal_state = initial;
    }
}

void app_state_enter_fault(void)
{
    if (system_state == SystemBoot || system_state == SystemRunning)
    {
        system_state = SystemFault;
    }
}

void app_state_update_thermal(ThermalState s)
{
    if (system_state == SystemRunning)
    {
        thermal_state = s;
    }
}
