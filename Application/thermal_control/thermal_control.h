#ifndef THERMAL_CONTROL_H
#define THERMAL_CONTROL_H

#include "app_mode.h"
#include "settings.h"

void        thermal_control_init(void);
SystemState thermal_control_step(SystemState current, const Settings *s);

#endif /* THERMAL_CONTROL_H */
