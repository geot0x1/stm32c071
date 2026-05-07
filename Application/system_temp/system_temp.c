#include "system_temp.h"
#include "temperature_sensor.h"
#include "hdc2010.h"

uint16_t system_temp_get(void)
{
    uint16_t ds = get_temperature();
    uint16_t hdc = hdc2010_get_temp();

    if (hdc == 0xFFFFU)
    {
        return ds;
    }
    if (ds == 0xFFFFU)
    {
        return hdc;
    }
    int16_t h = (int16_t)hdc;
    int16_t d = (int16_t)ds;
    return (uint16_t)(d > h ? d : h);
}
