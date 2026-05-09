#ifndef SYSTEM_TEMP_H
#define SYSTEM_TEMP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>

    /**
     * @brief Returns the system temperature as the max of DS18B20 and HDC2010.
     *        Returns 0xFFFF if no sensor is available.
     *        Value is in centidegrees (Celsius * 100), negatives as two's complement.
     */
    uint16_t system_temp_get(void);

#ifdef __cplusplus
}
#endif

#endif /* SYSTEM_TEMP_H */
