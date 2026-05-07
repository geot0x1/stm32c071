#include "hdc2010.h"
#include "stm32c0xx_hal.h"
#include <stddef.h>
#include <stdbool.h>

/* ── Register map ────────────────────────────────────────────────────────── */

#define REG_TEMP_LOW 0x00U
#define REG_TEMP_HIGH 0x01U
#define REG_HUM_LOW 0x02U
#define REG_HUM_HIGH 0x03U
#define REG_INTR_DRDY 0x04U
#define REG_TEMP_MAX 0x05U
#define REG_HUM_MAX 0x06U
#define REG_INTR_EN 0x07U
#define REG_TEMP_OFFSET 0x08U
#define REG_HUM_OFFSET 0x09U
#define REG_TEMP_THR_L 0x0AU
#define REG_TEMP_THR_H 0x0BU
#define REG_RH_THR_L 0x0CU
#define REG_RH_THR_H 0x0DU
#define REG_RST_DRDY_INT 0x0EU
#define REG_MEAS_CFG 0x0FU
#define REG_MFR_ID_LOW 0xFCU
#define REG_MFR_ID_HIGH 0xFDU
#define REG_DEV_ID_LOW 0xFEU
#define REG_DEV_ID_HIGH 0xFFU

/* Expected device ID: low byte 0xD0, high byte 0x07 → combined 0x07D0 */
#define HDC2010_DEV_ID_LOW 0xD0U
#define HDC2010_DEV_ID_HIGH 0x07U

/* MEAS_CFG: one-shot, temp + humidity, 14-bit resolution */
#define MEAS_CFG_ONESHOT 0x01U

#define I2C_TIMEOUT_MS 10U
#define HAL_ADDR(a) ((uint16_t)((a) << 1))

/* ── Module state ────────────────────────────────────────────────────────── */

static Hdc2010 dev;
static bool ok = false;
static bool valid = false;
static uint16_t last_temp = 0;
static uint8_t last_rh = 0;

/* ── Static helpers ──────────────────────────────────────────────────────── */

static Hdc2010Err read_reg(Hdc2010 *dev, uint8_t reg, uint8_t *out)
{
    I2cErr err = i2c_mem_read(
        dev->i2c, HAL_ADDR(dev->addr), reg, I2C_MEMADD_SIZE_8BIT, out, 1, I2C_TIMEOUT_MS);
    return (err == I2C_OK) ? HDC2010_OK : HDC2010_ERR_I2C;
}

static Hdc2010Err write_reg(Hdc2010 *dev, uint8_t reg, uint8_t val)
{
    I2cErr err = i2c_mem_write(
        dev->i2c, HAL_ADDR(dev->addr), reg, I2C_MEMADD_SIZE_8BIT, &val, 1, I2C_TIMEOUT_MS);
    return (err == I2C_OK) ? HDC2010_OK : HDC2010_ERR_I2C;
}

static Hdc2010Err hdc2010_start_measurement(Hdc2010 *dev)
{
    return write_reg(dev, REG_MEAS_CFG, MEAS_CFG_ONESHOT);
}

static Hdc2010Err hdc2010_read(Hdc2010 *dev, uint16_t *temperature_cdeg, uint8_t *humidity_pct)
{
    if (temperature_cdeg != NULL)
    {
        uint8_t lo, hi;
        if (read_reg(dev, REG_TEMP_LOW, &lo) != HDC2010_OK)
        {
            return HDC2010_ERR_I2C;
        }
        if (read_reg(dev, REG_TEMP_HIGH, &hi) != HDC2010_OK)
        {
            return HDC2010_ERR_I2C;
        }
        uint16_t raw = (uint16_t)((uint16_t)hi << 8) | lo;
        /* T(cdeg) = raw * 165 * 100 / 65536 - 4000 */
        *temperature_cdeg = (uint16_t)((int16_t)(((uint32_t)raw * 16500UL) / 65536UL) - 4000);
    }

    if (humidity_pct != NULL)
    {
        uint8_t lo, hi;
        if (read_reg(dev, REG_HUM_LOW, &lo) != HDC2010_OK)
        {
            return HDC2010_ERR_I2C;
        }
        if (read_reg(dev, REG_HUM_HIGH, &hi) != HDC2010_OK)
        {
            return HDC2010_ERR_I2C;
        }
        uint16_t raw = (uint16_t)((uint16_t)hi << 8) | lo;
        /* RH(%) = raw * 100 / 65536 */
        *humidity_pct = (uint8_t)(((uint32_t)raw * 100UL) / 65536UL);
    }

    return HDC2010_OK;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

Hdc2010Err hdc2010_init(I2c *i2c, uint8_t addr)
{
    dev.i2c = i2c;
    dev.addr = addr;

    uint8_t id_low, id_high;
    if (read_reg(&dev, REG_DEV_ID_LOW, &id_low) != HDC2010_OK
        || read_reg(&dev, REG_DEV_ID_HIGH, &id_high) != HDC2010_OK)
    {
        return HDC2010_ERR_NOT_FOUND;
    }
    if (id_low != HDC2010_DEV_ID_LOW || id_high != HDC2010_DEV_ID_HIGH)
    {
        return HDC2010_ERR_NOT_FOUND;
    }
    ok = true;
    return HDC2010_OK;
}

void hdc2010_task(void)
{
    typedef enum
    {
        Hdc2010Idle,
        Hdc2010Waiting
    } Hdc2010PollState;
    static Hdc2010PollState state = Hdc2010Idle;
    static uint32_t trigger_ms = 0U;
    static uint32_t poll_ms = 0U;

    if (!ok)
    {
        return;
    }

    uint32_t now = HAL_GetTick();

    switch (state)
    {
        case Hdc2010Idle:
            if (now - poll_ms >= 1000U)
            {
                hdc2010_start_measurement(&dev);
                trigger_ms = now;
                state = Hdc2010Waiting;
            }
            break;

        case Hdc2010Waiting:
            if (now - trigger_ms >= 2U)
            {
                uint16_t temp = 0;
                uint8_t rh = 0;
                if (hdc2010_read(&dev, &temp, &rh) == HDC2010_OK)
                {
                    last_temp = temp;
                    last_rh = rh;
                    valid = true;
                }
                poll_ms = now;
                state = Hdc2010Idle;
            }
            break;
    }
}

uint16_t hdc2010_get_temp(void)
{
    if (!ok || !valid)
    {
        return 0xFFFFU;
    }
    return last_temp;
}
uint8_t hdc2010_get_rh(void)
{
    if (!ok || !valid)
    {
        return 0xFFU;
    }
    return last_rh;
}
