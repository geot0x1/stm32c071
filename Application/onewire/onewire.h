#ifndef ONEWIRE_H
#define ONEWIRE_H
#ifdef __cplusplus
extern "C"
{
#endif

#include "stm32c0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

    typedef struct OneWire
    {
        GPIO_TypeDef *port;
        uint16_t pin;
        uint8_t ROM_NO[8];
        uint8_t LastDiscrepancy;
        uint8_t LastFamilyDiscrepancy;
        bool LastDeviceFlag;
        volatile uint32_t lock_count;
    } OneWire;


    uint8_t ow_crc8(const uint8_t *addr, uint8_t len);
    uint8_t ow_reset(OneWire *const ow);
    bool ow_search(OneWire *const ow, uint8_t *new_addr, bool search_mode);
    uint8_t ow_read_bit(OneWire *const ow);
    void ow_write(OneWire *const ow, uint8_t v);
    void ow_write_bytes(OneWire *const ow, const uint8_t *buf, uint16_t count);
    uint8_t ow_read(OneWire *const ow);
    void ow_select(OneWire *const ow, const uint8_t rom[8]);
    void ow_skip(OneWire *const ow);
    void ow_reset_search(OneWire *const ow);
    void ow_set_input(OneWire *ow);
    void ow_init(OneWire *ow);
    void ow_set_low(OneWire *ow);
    void ow_lock_bus(OneWire *ow);
    void ow_unlock_bus(OneWire *ow);
    uint8_t ow_pin_status(OneWire *const ow);

#ifdef __cplusplus
}
#endif
#endif
