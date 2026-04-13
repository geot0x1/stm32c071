#ifndef INTERNAL_TEMP_SENSOR_H
#define INTERNAL_TEMP_SENSOR_H

#include <stdint.h>

/**
 * @brief Status of the internal temperature sensor.
 */
typedef enum
{
    InternalTempSensorOk,      /**< Sensor read successfully */
    InternalTempSensorError    /**< Sensor read failed */
} InternalTempStatus;

/**
 * @brief Callback handler for internal temperature sensor events.
 */
typedef void (*InternalTempHandler)(InternalTempStatus status);

/**
 * @brief Initialize the internal temperature sensor module.
 *        Call once at startup before using other functions.
 */
void internal_temp_sensor_init(void);

/**
 * @brief Non-blocking state machine tick for the temperature sensor.
 *        Must be called frequently from the main loop (e.g., every 1-10 ms).
 *        Automatically triggers a new ADC read every ~2 seconds.
 */
void internal_temp_sensor_task(void);

/**
 * @brief Get the last valid temperature reading.
 *
 * @return Temperature in Celsius * 100 (e.g., 2500 = 25.00°C), or 0xFFFF on error/not-ready
 */
uint16_t internal_temp_sensor_get(void);

/**
 * @brief Register a handler callback for sensor events.
 *        The callback is invoked when a new measurement completes or fails.
 *
 * @param handler Callback function or NULL to disable
 */
void internal_temp_sensor_register_handler(InternalTempHandler handler);

#endif /* INTERNAL_TEMP_SENSOR_H */
