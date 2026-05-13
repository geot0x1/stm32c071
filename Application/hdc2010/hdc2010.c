#include "hdc2010.h"
#include "sys_time.h"
#include <stdbool.h>
#include <stddef.h>

/* ── Cached values ───────────────────────────────────────────────────────── */

static int16_t  cached_temp = INT16_MIN;
static uint8_t  cached_rh   = 0xFFU;
static bool     cached_valid = false;

/* ── Task state ──────────────────────────────────────────────────────────── */

typedef enum { Hdc2010Idle, Hdc2010Waiting } Hdc2010PollState;

static Hdc2010          *s_dev        = NULL;
static Hdc2010PollState  s_state      = Hdc2010Idle;
static millis_t          s_trigger_ms = 0U;
static millis_t          s_poll_ms    = 0U;

/* ── Register map ────────────────────────────────────────────────────────── */

#define REG_TEMP_LOW        0x00U
#define REG_TEMP_HIGH       0x01U
#define REG_HUM_LOW         0x02U
#define REG_HUM_HIGH        0x03U
#define REG_INTR_DRDY       0x04U
#define REG_TEMP_MAX        0x05U
#define REG_HUM_MAX         0x06U
#define REG_INTR_EN         0x07U
#define REG_TEMP_OFFSET     0x08U
#define REG_HUM_OFFSET      0x09U
#define REG_TEMP_THR_L      0x0AU
#define REG_TEMP_THR_H      0x0BU
#define REG_RH_THR_L        0x0CU
#define REG_RH_THR_H        0x0DU
#define REG_RST_DRDY_INT    0x0EU
#define REG_MEAS_CFG        0x0FU
#define REG_MFR_ID_LOW      0xFCU
#define REG_MFR_ID_HIGH     0xFDU
#define REG_DEV_ID_LOW      0xFEU
#define REG_DEV_ID_HIGH     0xFFU

/* Expected device ID: low byte 0xD0, high byte 0x07 → combined 0x07D0 */
#define HDC2010_DEV_ID_LOW  0xD0U
#define HDC2010_DEV_ID_HIGH 0x07U

/* MEAS_CFG: one-shot, temp + humidity, 14-bit resolution */
#define MEAS_CFG_ONESHOT    0x01U

#define I2C_TIMEOUT_MS      10U
#define HAL_ADDR(a)         ((uint16_t)((a) << 1))

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static Hdc2010Err read_reg(Hdc2010 *dev, uint8_t reg, uint8_t *out)
{
    I2cErr err = i2c_mem_read(dev->i2c, HAL_ADDR(dev->addr),
                              reg, I2C_MEMADD_SIZE_8BIT, out, 1, I2C_TIMEOUT_MS);
    return (err == I2C_OK) ? HDC2010_OK : HDC2010_ERR_I2C;
}

static Hdc2010Err write_reg(Hdc2010 *dev, uint8_t reg, uint8_t val)
{
    I2cErr err = i2c_mem_write(dev->i2c, HAL_ADDR(dev->addr),
                               reg, I2C_MEMADD_SIZE_8BIT, &val, 1, I2C_TIMEOUT_MS);
    return (err == I2C_OK) ? HDC2010_OK : HDC2010_ERR_I2C;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

Hdc2010Err hdc2010_init(Hdc2010 *dev, I2c *i2c, uint8_t addr)
{
    dev->i2c  = i2c;
    dev->addr = addr;

    uint8_t id_low, id_high;
    if (read_reg(dev, REG_DEV_ID_LOW, &id_low) != HDC2010_OK)
    {
        return HDC2010_ERR_NOT_FOUND;
    }
    if (read_reg(dev, REG_DEV_ID_HIGH, &id_high) != HDC2010_OK)
    {
        return HDC2010_ERR_NOT_FOUND;
    }

    if (id_low != HDC2010_DEV_ID_LOW || id_high != HDC2010_DEV_ID_HIGH)
    {
        return HDC2010_ERR_NOT_FOUND;
    }

    s_dev = dev;
    return HDC2010_OK;
}

Hdc2010Err hdc2010_start_measurement(Hdc2010 *dev)
{
    return write_reg(dev, REG_MEAS_CFG, MEAS_CFG_ONESHOT);
}

Hdc2010Err hdc2010_read(Hdc2010 *dev, int16_t *temperature_cdeg, uint8_t *humidity_pct)
{
    int16_t temp_value = 0;
    uint8_t rh_value = 0;

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
        temp_value = (int16_t)(((uint32_t)raw * 16500UL) / 65536UL) - 4000;
        *temperature_cdeg = temp_value;
        cached_temp = temp_value;
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
        rh_value = (uint8_t)(((uint32_t)raw * 100UL) / 65536UL);
        *humidity_pct = rh_value;
        cached_rh = rh_value;
    }

    cached_valid = true;
    return HDC2010_OK;
}

int16_t hdc2010_get_temp(void)
{
    if (!cached_valid)
    {
        return INT16_MIN;
    }
    return cached_temp;
}

uint8_t hdc2010_get_rh(void)
{
    if (!cached_valid)
    {
        return 0xFFU;
    }
    return cached_rh;
}

void hdc2010_task(void)
{
    if (s_dev == NULL)
    {
        return;
    }

    millis_t now = millis();

    switch (s_state)
    {
        case Hdc2010Idle:
            if (now - s_poll_ms >= 1000U)
            {
                hdc2010_start_measurement(s_dev);
                s_trigger_ms = now;
                s_state      = Hdc2010Waiting;
            }
            break;

        case Hdc2010Waiting:
            if (now - s_trigger_ms >= 2U)
            {
                int16_t temp = 0;
                uint8_t rh   = 0;
                hdc2010_read(s_dev, &temp, &rh);
                s_poll_ms = now;
                s_state   = Hdc2010Idle;
            }
            break;
    }
}
