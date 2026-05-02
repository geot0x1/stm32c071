#include "i2c.h"
#include <string.h>

static I2cErr hal_to_i2c_err(HAL_StatusTypeDef status)
{
    switch (status)
    {
        case HAL_OK:
            return I2C_OK;
        case HAL_TIMEOUT:
            return I2C_ERR_TIMEOUT;
        case HAL_BUSY:
            return I2C_ERR_BUSY;
        default:
            return I2C_ERR_HAL;
    }
}

void i2c_init(I2c *i2c, I2C_TypeDef *instance, uint32_t timing)
{
    memset(i2c, 0, sizeof(I2c));

    i2c->hal_handle.Instance = instance;
    i2c->hal_handle.Init.Timing = timing;
    i2c->hal_handle.Init.OwnAddress1 = 0;
    i2c->hal_handle.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    i2c->hal_handle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    i2c->hal_handle.Init.OwnAddress2 = 0;
    i2c->hal_handle.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    i2c->hal_handle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    i2c->hal_handle.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;

    /* HAL_I2C_MspInit (in stm32c0xx_hal_msp.c) is called automatically
     * during HAL_I2C_Init and handles GPIO AF pins and clock enable. */
    HAL_I2C_Init(&i2c->hal_handle);
    HAL_I2CEx_ConfigAnalogFilter(&i2c->hal_handle, I2C_ANALOGFILTER_ENABLE);
    HAL_I2CEx_ConfigDigitalFilter(&i2c->hal_handle, 0);
}

I2cErr i2c_write(
    I2c *i2c, uint16_t dev_addr, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return hal_to_i2c_err(
        HAL_I2C_Master_Transmit(&i2c->hal_handle, dev_addr, (uint8_t *)data, len, timeout_ms));
}

I2cErr i2c_read(I2c *i2c, uint16_t dev_addr, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return hal_to_i2c_err(
        HAL_I2C_Master_Receive(&i2c->hal_handle, dev_addr, data, len, timeout_ms));
}

I2cErr i2c_mem_write(I2c *i2c, uint16_t dev_addr, uint16_t mem_addr, uint16_t mem_addr_size,
    const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return hal_to_i2c_err(HAL_I2C_Mem_Write(
        &i2c->hal_handle, dev_addr, mem_addr, mem_addr_size, (uint8_t *)data, len, timeout_ms));
}

I2cErr i2c_mem_read(I2c *i2c, uint16_t dev_addr, uint16_t mem_addr, uint16_t mem_addr_size,
    uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return hal_to_i2c_err(HAL_I2C_Mem_Read(
        &i2c->hal_handle, dev_addr, mem_addr, mem_addr_size, data, len, timeout_ms));
}
