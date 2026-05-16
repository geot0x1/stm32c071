#ifndef THERMAL_CONTROL_H
#define THERMAL_CONTROL_H

#include "app_state.h"
#include "settings.h"
#include <stdint.h>

void         thermal_control_init(void);
ThermalState thermal_control_initial(int16_t t_cdeg, const Settings *s);
ThermalState thermal_control_step(ThermalState current, int16_t t_cdeg, const Settings *s);

#endif /* THERMAL_CONTROL_H */
