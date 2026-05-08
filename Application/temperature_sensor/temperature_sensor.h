#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#include <stdint.h>

typedef enum
{
    SensorLost,
    AboveA,
    AboveB,
    BelowA,
    BelowB
} TempSensorEvent;

typedef void (*TempSensorHandler)(TempSensorEvent event);

/**
 * @brief  Initializes the temperature sensor module.
 */
void temperature_sensor_init(void);

/**
 * @brief  State machine tick function, should be called in the main loop.
 *         Handles non-blocking polling, conversion, and reading.
 */
void temperature_sensor_task(void);

/**
 * @brief  Returns the last valid temperature raw value.
 *         Returns 0xFFFF if the sensor is lost for 3 consecutive cycles.
 */
uint16_t get_temperature(void);

/**
 * @brief  Setters for temperature setpoints and hysteresis (Celsius * 100).
 */
void temperature_sensor_set_setpoint_a(uint16_t setpoint);
void temperature_sensor_set_setpoint_b(uint16_t setpoint);
void temperature_sensor_set_hysteresis(uint16_t hysteresis);

/**
 * @brief  Registers a handler for temperature sensor events.
 */
void temperature_sensor_register_handler(TempSensorHandler handler);

#endif /* TEMPERATURE_SENSOR_H */
