#ifndef BSP_I2C_H
#define BSP_I2C_H

#include <stdint.h>
#include "stm32c0xx_hal.h"

typedef struct I2c_s
{
    I2C_HandleTypeDef hal_handle;
} I2c;

typedef enum
{
    I2C_OK = 0,
    I2C_ERR_TIMEOUT,
    I2C_ERR_BUSY,
    I2C_ERR_HAL
} I2cErr;

/**
 * @brief Initialize an I2C master peripheral.
 *
 * GPIO pin AF configuration is handled by HAL_I2C_MspInit in
 * stm32c0xx_hal_msp.c (called automatically by HAL_I2C_Init).
 *
 * @param i2c      Handle to fill
 * @param instance I2C peripheral (I2C1, I2C2, …)
 * @param timing   Value for I2C_TIMINGR register (from STM32CubeMX)
 */
void i2c_init(I2c *i2c, I2C_TypeDef *instance, uint32_t timing);

/**
 * @brief Transmit bytes to a device.
 */
I2cErr i2c_write(I2c *i2c, uint16_t dev_addr,
                    const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Receive bytes from a device.
 */
I2cErr i2c_read(I2c *i2c, uint16_t dev_addr,
                   uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Write to a device register / memory address.
 */
I2cErr i2c_mem_write(I2c *i2c, uint16_t dev_addr, uint16_t mem_addr,
                         uint16_t mem_addr_size, const uint8_t *data,
                         uint16_t len, uint32_t timeout_ms);

/**
 * @brief Read from a device register / memory address.
 */
I2cErr i2c_mem_read(I2c *i2c, uint16_t dev_addr, uint16_t mem_addr,
                        uint16_t mem_addr_size, uint8_t *data,
                        uint16_t len, uint32_t timeout_ms);

#endif /* BSP_I2C_H */
