#ifndef HDC2010_H
#define HDC2010_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "i2c.h"

/* 7-bit I2C addresses (ADDR pin selects) */
#define HDC2010_ADDR_LOW    0x40U
#define HDC2010_ADDR_HIGH   0x41U

typedef enum
{
    HDC2010_OK = 0,
    HDC2010_ERR_I2C,
    HDC2010_ERR_NOT_FOUND,
} Hdc2010Err;

typedef struct
{
    I2c    *i2c;
    uint8_t addr;
} Hdc2010;

/**
 * @brief Initialize and verify the HDC2010 by reading its device ID.
 *
 * @param dev   Handle to fill
 * @param i2c   Initialized I2C bus
 * @param addr  7-bit device address (HDC2010_ADDR_LOW or HDC2010_ADDR_HIGH)
 * @return HDC2010_ERR_NOT_FOUND if the device does not respond or returns an
 *         unexpected ID.
 */
Hdc2010Err hdc2010_init(Hdc2010 *dev, I2c *i2c, uint8_t addr);

/**
 * @brief Trigger a one-shot temperature + humidity measurement.
 *
 * The conversion takes up to 1 ms. The caller must wait before calling
 * hdc2010_read().
 */
Hdc2010Err hdc2010_start_measurement(Hdc2010 *dev);

/**
 * @brief Read the last completed measurement results.
 *
 * @param dev              Device handle
 * @param temperature_cdeg Celsius * 100 as signed int16 (e.g. 2350 = 23.50 C).
 *                         Pass NULL to skip.
 * @param humidity_pct     Relative humidity 0-100 %. Pass NULL to skip.
 */
Hdc2010Err hdc2010_read(Hdc2010 *dev, int16_t *temperature_cdeg, uint8_t *humidity_pct);

/**
 * @brief Get the last cached temperature value.
 *
 * Returns 0xFFFF if no valid reading is available.
 * Value is in centidegrees (Celsius * 100), negatives as two's complement.
 */
uint16_t hdc2010_get_temp(void);

/**
 * @brief Get the last cached humidity value.
 *
 * Returns 0xFF if no valid reading is available.
 * Value is relative humidity 0-100%.
 */
uint8_t hdc2010_get_rh(void);

/**
 * @brief Cooperative polling task — call once per main loop iteration.
 *
 * Triggers a one-shot measurement every second and reads the result after
 * the conversion delay. Does nothing if hdc2010_init() was not called or
 * returned an error.
 */
void hdc2010_task(void);

#ifdef __cplusplus
}
#endif

#endif /* HDC2010_H */
