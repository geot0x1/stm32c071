#ifndef HDC2010_H
#define HDC2010_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "i2c.h"
#include <stdint.h>

/* 7-bit I2C addresses (ADDR pin selects) */
#define HDC2010_ADDR_LOW 0x40U
#define HDC2010_ADDR_HIGH 0x41U

    typedef enum
    {
        HDC2010_OK = 0,
        HDC2010_ERR_I2C,
        HDC2010_ERR_NOT_FOUND,
    } Hdc2010Err;

    typedef struct
    {
        I2c *i2c;
        uint8_t addr;
    } Hdc2010;

    Hdc2010Err hdc2010_init(I2c *i2c, uint8_t addr);
    void hdc2010_task(void);
    uint16_t hdc2010_get_temp(void);  /* returns 0xFFFF when not ready */
    uint8_t hdc2010_get_rh(void);     /* returns 0xFF when not ready */

#ifdef __cplusplus
}
#endif

#endif /* HDC2010_H */
