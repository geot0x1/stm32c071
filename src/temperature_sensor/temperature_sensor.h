#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <stdint.h>

/**
 * @brief  Initializes the temperature sensor module.
 */
void temperature_sensor_init(void);

/**
 * @brief  State machine tick function, should be called in the main loop.
 *         Handles non-blocking polling, conversion, and reading.
 */
void temperature_sensor_tick(void);

/**
 * @brief  Returns the last valid temperature raw value.
 *         Returns 0xFFFF if the sensor is lost for 3 consecutive cycles.
 */
uint16_t get_temperature(void);

#endif /* TEMPERATURE_SENSOR_H */
