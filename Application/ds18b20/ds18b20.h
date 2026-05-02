#ifndef DS18B20_H
#define DS18B20_H
#ifdef __cplusplus
extern "C"
{
#endif


#include "onewire.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define DS18B20_ADDRESS_SIZE (8)

    typedef struct
    {
        OneWire *ow;
        uint8_t devices;
        uint8_t ds18Count;
        uint8_t bitResolution;
        bool parasite;
        bool autoSaveScratchPad;
    } Ds18b20_t;

    void ds18b20_begin(Ds18b20_t *ds);

    // Returns true if address is valid
    bool ds18b20_valid_address(const uint8_t *device_address);

    // Returns true if address is of the family of sensors the lib supports.
    bool ds18b20_valid_family(const uint8_t *device_address);

    // Read device's power requirements
    bool ds18b20_read_power_supply(Ds18b20_t *ds, const uint8_t *device_address);

    // Returns the device resolution: 9, 10, 11, or 12 bits
    uint8_t ds18b20_get_resolution(Ds18b20_t *ds, const uint8_t *device_address);

    // Attempt to determine if the device at the given address is connected to the bus
    // Also allows for updating the read scratchpad
    bool ds18b20_is_connected(Ds18b20_t *ds, const uint8_t *device_address, uint8_t *scratch_pad);

    // Returns true if all bytes of a buff are '\0'
    bool ds18b20_is_all_zeros(const uint8_t *const buff, const size_t length);

    // Read device's scratchpad
    bool ds18b20_read_scratch_pad(
        Ds18b20_t *ds, const uint8_t *device_address, uint8_t *scratch_pad);

    // Write device's scratchpad
    void ds18b20_write_scratch_pad(
        Ds18b20_t *ds, const uint8_t *device_address, const uint8_t *scratch_pad);

    // Finds an address at a given index on the bus
    bool ds18b20_get_address(Ds18b20_t *ds, uint8_t *device_address, uint8_t index);

    // Returns temperature raw value (12-bit integer of 1/128 degrees C)
    int16_t ds18b20_get_temp(Ds18b20_t *ds, const uint8_t *device_address);

    /**
     * @brief Convert from raw to Celsius
     *
     * @param raw value read from temperature sensor
     * @return float -127 if raw value is <= -7040.
     *         otherwise returns the converted value.
     */
    float ds18b20_raw_to_celsius(int16_t raw);

    /**
     * @brief Sends command for all devices on the bus to perform a temperature conversion
     *
     * @param ds Pointer to ds18b20 data
     * @return int 0 on success. -1 if it fails.
     */
    int ds18b20_request_temperatures(Ds18b20_t *ds);

    uint8_t ds18b20_get_sensors_on_bus(Ds18b20_t *ds);

    /**
     * @brief set resolution of a device to 9, 10, 11, or 12 bits
     * if new resolution is out of range, 9 bits is used.
     * @param ds Pointer to ds18b20 data
     * @param deviceAddress Pointer to device address
     * @param newResolution 9, 10, 11, or 12 bits
     * @param skipGlobalBitResolutionCalculation if true, the global bit resolution is not updated
     */
    bool ds18b20_set_resolution(Ds18b20_t *ds, const uint8_t *deviceAddress, uint8_t newResolution,
        bool skipGlobalBitResolutionCalculation);

#ifdef __cplusplus
}
#endif
#endif
