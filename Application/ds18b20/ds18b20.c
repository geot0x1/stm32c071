#include "ds18b20.h"
#include <string.h>

// OneWire commands
#define STARTCONVO                 0x44 // Tells device to take a temperature reading and put it on the scratchpad
#define COPYSCRATCH                0x48 // Copy scratchpad to EEPROM
#define READSCRATCH                0xBE // Read from scratchpad
#define WRITESCRATCH               0x4E // Write to scratchpad
#define RECALLSCRATCH              0xB8 // Recall from EEPROM to scratchpad
#define READPOWERSUPPLY            0xB4 // Determine if device needs parasite power
#define ALARMSEARCH                0xEC // Query bus for devices with an alarm condition

#define DEVICE_ADDRESS_SIZE        8
#define SCRATCHPAD_SIZE            9

// DSROM FIELDS
#define DSROM_FAMILY               0
#define DSROM_CRC                  7

// Model IDs
#define DS18S20MODEL               0x10 // also DS1820
#define DS18B20MODEL               0x28 // also MAX31820
#define DS1822MODEL                0x22
#define DS1825MODEL                0x3B
#define DS28EA00MODEL              0x42

// Device resolution
#define TEMP_9_BIT                 0x1F //  9 bit
#define TEMP_10_BIT                0x3F // 10 bit
#define TEMP_11_BIT                0x5F // 11 bit
#define TEMP_12_BIT                0x7F // 12 bit

// Scratchpad locations
#define TEMP_LSB                   0
#define TEMP_MSB                   1
#define HIGH_ALARM_TEMP            2
#define LOW_ALARM_TEMP             3
#define CONFIGURATION              4
#define INTERNAL_BYTE              5
#define COUNT_REMAIN               6
#define COUNT_PER_C                7
#define SCRATCHPAD_CRC             8

// Error Codes
#define DEVICE_DISCONNECTED_C      -127
#define DEVICE_DISCONNECTED_F      -196.6
#define DEVICE_DISCONNECTED_RAW    -7040

#define MAX_CONVERSION_TIMEOUT     750


static int16_t calculate_temperature(const uint8_t *deviceAddress, uint8_t *scratchPad);
static uint8_t ds18b20_check_crc(const uint8_t *buf, uint8_t len, uint8_t crc);


void ds18b20_begin(Ds18b20_t * ds)
{
	uint8_t device_address_array[2][DEVICE_ADDRESS_SIZE];
    memset(device_address_array, 0, sizeof(device_address_array));
    ow_init(ds->ow);
    ow_lock_bus(ds->ow);
	ow_reset_search(ds->ow);


	ds->devices   = 0; // Reset the number of devices when we enumerate wire devices
	ds->ds18Count = 0; // Reset number of DS18xxx Family devices

	uint8_t* device_address   = &device_address_array[0][0];
	uint8_t* previous_address = &device_address_array[1][0];
	while (ow_search(ds->ow, device_address, true)) //edw
	{
		if (memcmp(previous_address, device_address, DEVICE_ADDRESS_SIZE) != 0)
		{
			memcpy(previous_address, device_address, DEVICE_ADDRESS_SIZE);
		}
		else
		{
			continue;
		}
		if (ds18b20_valid_address(device_address))
		{
			ds->devices++;
			if (ds18b20_valid_family(device_address))
			{
				ds->ds18Count++;
			}
		}
	}
    ow_unlock_bus(ds->ow);
}

uint8_t ds18b20_get_sensors_on_bus(Ds18b20_t* ds)
{
	if (ds)
	{
		return ds->ds18Count;
	}
	return 0;
}

// returns true if address is valid
bool ds18b20_valid_address(const uint8_t *device_address)
{
    if (ds18b20_is_all_zeros(device_address, 8))
    {
        return false;
    }
	return ds18b20_check_crc(device_address, 7, device_address[DSROM_CRC]) == 0;
}

bool ds18b20_valid_family(const uint8_t *deviceAddress)
{
	switch (deviceAddress[DSROM_FAMILY])
	{
		case DS18S20MODEL:
		case DS18B20MODEL:
		case DS1822MODEL:
		case DS1825MODEL:
		case DS28EA00MODEL:
		{
			return true;
		}

		default:
		{
			return false;
		}
	}
}

bool ds18b20_read_power_supply(Ds18b20_t * ds, const uint8_t *device_address)
{
	bool parasiteMode = false;

	ow_reset(ds->ow);
	if (device_address == NULL)
	{
		ow_skip(ds->ow);
	}
	else
	{
		ow_select(ds->ow, device_address);
	}

	ow_write(ds->ow, READPOWERSUPPLY);
	if (ow_read_bit(ds->ow) == 0)
	{
		parasiteMode = true;
	}
	ow_reset(ds->ow);
	return parasiteMode;
}

// returns the current resolution of the device, 9-12
// returns 0 if device not found
uint8_t ds18b20_get_resolution(Ds18b20_t * ds, const uint8_t *device_address)
{
	if (device_address[DSROM_FAMILY] == DS18S20MODEL)
	{
		return 12;
	}

	uint8_t scratchPad[SCRATCHPAD_SIZE];

	if (ds18b20_is_connected(ds, device_address, scratchPad))
	{
		switch (scratchPad[CONFIGURATION])
		{
			case TEMP_12_BIT:
			{
				return 12;
			}

			case TEMP_11_BIT:
			{
				return 11;
			}

			case TEMP_10_BIT:
			{
				return 10;
			}

			case TEMP_9_BIT:
			{
				return 9;
			}
		}
	}
	return 0;
}

// attempt to determine if the device at the given address is connected to the bus
// also allows for updating the read scratchpad
bool ds18b20_is_connected(Ds18b20_t * ds, const uint8_t *device_address,
                          uint8_t *scratch_pad)
{
	if (!ds18b20_read_scratch_pad(ds, device_address, scratch_pad))
	{
		return false;
	}

    if (ds18b20_is_all_zeros(scratch_pad, SCRATCHPAD_SIZE))
    {
        return false;
    }

    if (ow_crc8(scratch_pad, SCRATCHPAD_SIZE - 1) != scratch_pad[SCRATCHPAD_CRC])
    {
        return false;
    }

    return true;
}

// Returns true if all bytes of a buff are '\0'
bool ds18b20_is_all_zeros(const uint8_t * const buff, const size_t length)
{
	for (size_t i = 0; i < length; i++)
	{
		if (buff[i] != 0)
		{
			return false;
		}
	}

	return true;
}

bool ds18b20_read_scratch_pad(Ds18b20_t * ds, const uint8_t *deviceAddress, uint8_t *scratchPad)
{
    ow_lock_bus(ds->ow);
    while (!ow_pin_status(ds->ow))
    {
    }
	// Send the reset command and fail fast
	int b = ow_reset(ds->ow);

	if (b == 0)
	{
        ow_unlock_bus(ds->ow);
		return false;
	}

    while (!ow_pin_status(ds->ow))
    {
    }

    if (deviceAddress == NULL)
    {
        ow_skip(ds->ow);
    }
    else
    {
        ow_select(ds->ow, deviceAddress);
    }
    ow_write(ds->ow, READSCRATCH);

    while (!ow_pin_status(ds->ow))
    {
    }

	// Read all registers in a simple loop
	for (uint8_t i = 0; i < 9; i++)
	{
		scratchPad[i] = ow_read(ds->ow);
	}

    ow_unlock_bus(ds->ow);
    return true;
}

bool ds18b20_set_resolution(Ds18b20_t * ds, const uint8_t *deviceAddress,
                            uint8_t newResolution, bool skipGlobalBitResolutionCalculation)
{
	bool success = false;

	// DS1820 and DS18S20 have no resolution configuration register
	if (deviceAddress[DSROM_FAMILY] == DS18S20MODEL)
	{
		success = true;
	}
	else
	{
		// handle the sensors with configuration register
		if (newResolution < 9)
		{
			newResolution = 9;
		}
		else if (newResolution > 12)
		{
			newResolution = 12;
		}
		uint8_t newValue = 0;
		uint8_t scratchPad[SCRATCHPAD_SIZE];

		// we can only update the sensor if it is connected
		if (ds18b20_is_connected(ds, deviceAddress, scratchPad))
		{
			switch (newResolution)
			{
				case 12:
				{
					newValue = TEMP_12_BIT;
					break;
				}

				case 11:
				{
					newValue = TEMP_11_BIT;
					break;
				}

				case 10:
				{
					newValue = TEMP_10_BIT;
					break;
				}

				case 9:
				default:
				{
					newValue = TEMP_9_BIT;
					break;
				}
			}

			// if it needs to be updated we write the new value
			if (scratchPad[CONFIGURATION] != newValue)
			{
				scratchPad[CONFIGURATION] = newValue;
				ds18b20_write_scratch_pad(ds, deviceAddress, scratchPad);
			}
			// done
			success = true;
		}
	}

	// do we need to update the max resolution used?
	if (skipGlobalBitResolutionCalculation == false)
	{
		ds->bitResolution = newResolution;
		if (ds->devices > 1)
		{
			for (uint8_t i = 0; i < ds->devices; i++)
			{
				if (ds->bitResolution == 12)
				{
					break;
				}
				uint8_t deviceAddr[DEVICE_ADDRESS_SIZE];
                memset(deviceAddr, 0, DEVICE_ADDRESS_SIZE);
				ds18b20_get_address(ds, deviceAddr, i);
				uint8_t b = ds18b20_get_resolution(ds, deviceAddr);
				if (b > ds->bitResolution)
				{
					ds->bitResolution = b;
				}
			}
		}
	}

	return success;
}

// finds an address at a given index on the bus
// returns true if the device was found
bool ds18b20_get_address(Ds18b20_t * ds, uint8_t *deviceAddress, uint8_t index)
{
	uint8_t depth = 0;
    
    ow_lock_bus(ds->ow);
	ow_reset_search(ds->ow);

    while (!ow_pin_status(ds->ow))
    {
    }

	while (ow_search(ds->ow, deviceAddress, true))
	{
		if (ds18b20_valid_family(deviceAddress) && ds18b20_valid_address(deviceAddress)) 
		{
			if (depth == index)
			{
                ow_unlock_bus(ds->ow);
				return true;
			}
			depth++;
		}
	}

    ow_unlock_bus(ds->ow);
	return false;
}

void ds18b20_write_scratch_pad(Ds18b20_t * ds, const uint8_t *deviceAddress, const uint8_t *scratchPad)
{
	ow_reset(ds->ow);
	ow_select(ds->ow, deviceAddress);
	ow_write(ds->ow, WRITESCRATCH);
	ow_write(ds->ow, scratchPad[HIGH_ALARM_TEMP]); // high alarm temp
	ow_write(ds->ow, scratchPad[LOW_ALARM_TEMP]); // low alarm temp

	if (deviceAddress[DSROM_FAMILY] != DS18S20MODEL)
	{
		ow_write(ds->ow, scratchPad[CONFIGURATION]);
	}

	ow_reset(ds->ow);
}

// returns temperature in 1/128 degrees C or DEVICE_DISCONNECTED_RAW if the
// device's scratch pad cannot be read successfully.
// the numeric value of DEVICE_DISCONNECTED_RAW is defined in
// DallasTemperature.h. It is a large negative number outside the
// operating range of the device
int16_t ds18b20_get_temp(Ds18b20_t * ds, const uint8_t *deviceAddress)
{
	uint8_t scratchPad[SCRATCHPAD_SIZE];
    memset(scratchPad, 0, SCRATCHPAD_SIZE);

	if (ds18b20_is_connected(ds, deviceAddress, scratchPad))
	{
		return calculate_temperature(deviceAddress, scratchPad);
	}
	return DEVICE_DISCONNECTED_RAW;
}

// reads scratchpad and returns fixed-point temperature, scaling factor 2^-7
static int16_t calculate_temperature(const uint8_t *deviceAddress, uint8_t *scratchPad)
{
	int16_t fpTemperature = (((int16_t)scratchPad[TEMP_MSB]) << 11)
	                        | (((int16_t)scratchPad[TEMP_LSB]) << 3);

	/*
	 * DS1820 and DS18S20 have a 9-bit temperature register.
	 *
	 * Resolutions greater than 9-bit can be calculated using the data from
	 * the temperature, and COUNT REMAIN and COUNT PER °C registers in the
	 * scratchpad.  The resolution of the calculation depends on the model.
	 *
	 * While the COUNT PER °C register is hard-wired to 16 (10h) in a
	 * DS18S20, it changes with temperature in DS1820.
	 *
	 * After reading the scratchpad, the TEMP_READ value is obtained by
	 * truncating the 0.5°C bit (bit 0) from the temperature data. The
	 * extended resolution temperature can then be calculated using the
	 * following equation:
	 *
	 *                                COUNT_PER_C - COUNT_REMAIN
	 * TEMPERATURE = TEMP_READ - 0.25 + --------------------------
	 *                                       COUNT_PER_C
	 *
	 * Hagai Shatz simplified this to integer arithmetic for a 12 bits
	 * value for a DS18S20, and James Cameron added legacy DS1820 support.
	 *
	 * See - http://myarduinotoy.blogspot.co.uk/2013/02/12bit-result-from-ds18s20.html
	 */

	if ((deviceAddress[DSROM_FAMILY] == DS18S20MODEL) && (scratchPad[COUNT_PER_C] != 0))
	{
		fpTemperature = ((fpTemperature & 0xfff0) << 3) - 32
		                + (((scratchPad[COUNT_PER_C] - scratchPad[COUNT_REMAIN]) << 7)
		                   / scratchPad[COUNT_PER_C]);
	}

	return fpTemperature;
}

float ds18b20_raw_to_celsius(int16_t raw)
{
	if (raw <= DEVICE_DISCONNECTED_RAW)
	{
		return DEVICE_DISCONNECTED_C;
	}
	//! C = RAW/128
	return (float)raw * 0.0078125f;
}

// sends command for all devices on the bus to perform a temperature conversion
int ds18b20_request_temperatures(Ds18b20_t * ds)
{
	if (!ow_reset(ds->ow))
    {
        return -1;
    }
	ow_skip(ds->ow);
	ow_write(ds->ow, STARTCONVO);
	return 0;
}

static uint8_t ds18b20_check_crc(const uint8_t *buf, uint8_t len, uint8_t crc)
{
    uint8_t crc8 = ow_crc8(buf, len); /* calculate crc8 */

    if (crc8 == crc)                  /* check crc */
    {
        return 0; /* return right */
    }
    else
    {
        return 1; /* return wrong */
    }
}