#include "onewire.h"
#include "critical.h"
#include "delay.h"

#define ENTER_CRITICAL() critical_enter()
#define EXIT_CRITICAL() critical_exit()

#define TIME_DELAY_A (3)
#define TIME_DELAY_B (60)
#define TIME_DELAY_C (60)
#define TIME_DELAY_D (10)
#define TIME_DELAY_E (10)
#define TIME_DELAY_F (55)
#define TIME_DELAY_G (0)
#define TIME_DELAY_H (480)
#define TIME_DELAY_I (70)
#define TIME_DELAY_J (410)

static void ow_sem_lock(OneWire *ow);
static void ow_sem_unlock(OneWire *ow);
static void ow_write_bit(OneWire *const ow, uint8_t v);

static void ow_enable_gpio_clock(GPIO_TypeDef *port)
{
    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    else if (port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    else if (port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
#if defined(GPIOD)
    else if (port == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
#endif
#if defined(GPIOF)
    else if (port == GPIOF)
    {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    }
#endif
}

static inline void set_pin_output(GPIO_TypeDef *port, const uint16_t pin)
{
    uint32_t pin_pos = __builtin_ctz(pin);

    // Open-Drain (1)
    port->OTYPER |= pin;

    // High Speed (11)
    port->OSPEEDR |= (3U << (pin_pos * 2));

    // Pull-up (01)
    port->PUPDR &= ~(3U << (pin_pos * 2));
    port->PUPDR |= (1U << (pin_pos * 2));

    // Output Mode (01)
    port->MODER &= ~(3U << (pin_pos * 2));
    port->MODER |= (1U << (pin_pos * 2));
}

static inline void set_pin_input(GPIO_TypeDef *port, const uint16_t pin)
{
    // Keeping pin in Output Open-Drain mode is sufficient for reading.
    // However, ensure it's high impedance by setting BSRR.
    port->BSRR = pin;
}

static inline bool read_pin(GPIO_TypeDef *port, const uint16_t pin)
{
    return (port->IDR & pin) != 0;
}

static inline void ow_pin_set_high(OneWire *ow)
{
    ow->port->BSRR = ow->pin;
}

static inline void ow_pin_set_low(OneWire *ow)
{
    ow->port->BSRR = (uint32_t)ow->pin << 16;
}

static inline bool ow_pin_read(OneWire *ow)
{
    return read_pin(ow->port, ow->pin);
}

void ow_set_low(OneWire *ow)
{
    ow_sem_lock(ow);
    ow_pin_set_low(ow);
    ow_sem_unlock(ow);
}

void ow_init(OneWire *ow)
{
    ow->lock_count = 0;
    ow_enable_gpio_clock(ow->port);
    ow_sem_lock(ow);

    set_pin_output(ow->port, ow->pin);
    ow_pin_set_high(ow); /* Initial state High (released) */

    ow_sem_unlock(ow);
}

static void ow_sem_lock(OneWire *ow)
{
    ENTER_CRITICAL();
    ow->lock_count++;
    EXIT_CRITICAL();
}

static void ow_sem_unlock(OneWire *ow)
{
    ENTER_CRITICAL();
    if (ow->lock_count > 0)
    {
        ow->lock_count--;
    }
    EXIT_CRITICAL();
}

uint8_t ow_reset(OneWire *const ow)
{
    ow_sem_lock(ow);
    uint8_t err = 0;

    ENTER_CRITICAL();

    ow_pin_set_low(ow);

    delay_us(TIME_DELAY_H);

    ow_pin_set_high(ow);
    delay_us(TIME_DELAY_I);

    err = !ow_pin_read(ow);

    delay_us(TIME_DELAY_J);

    EXIT_CRITICAL();

    ow_sem_unlock(ow);

    return err;
}

void ow_reset_search(OneWire *const ow)
{
    ow->LastDiscrepancy = 0;
    ow->LastDeviceFlag = false;
    ow->LastFamilyDiscrepancy = 0;
    for (int i = 7; i >= 0; i--)
    {
        ow->ROM_NO[i] = 0;
    }
}

static void ow_write_bit(OneWire *const ow, uint8_t v)
{
    ow_sem_lock(ow);
    if (v & 1)
    {
        ENTER_CRITICAL();
        ow_pin_set_low(ow);
        delay_us(TIME_DELAY_A);

        ow_pin_set_high(ow);
        delay_us(TIME_DELAY_B);
        EXIT_CRITICAL();
    }
    else
    {
        ENTER_CRITICAL();
        ow_pin_set_low(ow);
        delay_us(TIME_DELAY_C);

        ow_pin_set_high(ow);
        delay_us(TIME_DELAY_D);
        EXIT_CRITICAL();
    }
    ow_sem_unlock(ow);
}

uint8_t ow_pin_status(OneWire *const ow)
{
    return ow_pin_read(ow) ? 1 : 0;
}

uint8_t ow_read_bit(OneWire *const ow)
{
    ow_sem_lock(ow);

    uint8_t r;

    ENTER_CRITICAL();

    ow_pin_set_low(ow);
    delay_us(TIME_DELAY_A);

    ow_pin_set_high(ow);
    delay_us(TIME_DELAY_E);

    r = ow_pin_read(ow);

    delay_us(TIME_DELAY_F);

    EXIT_CRITICAL();

    ow_sem_unlock(ow);
    return r;
}

void ow_write(OneWire *const ow, uint8_t v)
{
    ow_sem_lock(ow);
    for (uint8_t bit_mask = 0x01; bit_mask; bit_mask <<= 1)
    {
        ow_write_bit(ow, (bit_mask & v) ? 1 : 0);
    }
    ow_sem_unlock(ow);
}

void ow_write_bytes(OneWire *const ow, const uint8_t *buf, uint16_t count)
{
    ow_sem_lock(ow);
    for (uint16_t i = 0; i < count; i++)
    {
        ow_write(ow, buf[i]);
    }
    ow_sem_unlock(ow);
}

uint8_t ow_read(OneWire *const ow)
{
    uint8_t bit_mask;
    uint8_t r = 0;
    ow_sem_lock(ow);
    for (bit_mask = 0x01; bit_mask; bit_mask <<= 1)
    {
        if (ow_read_bit(ow))
        {
            r |= bit_mask;
        }
    }
    ow_sem_unlock(ow);
    return r;
}

void ow_select(OneWire *const ow, const uint8_t rom[8])
{
    ow_sem_lock(ow);
    ow_write(ow, 0x55);
    for (uint8_t i = 0; i < 8; i++)
    {
        ow_write(ow, rom[i]);
    }
    ow_sem_unlock(ow);
}

void ow_skip(OneWire *const ow)
{
    ow_sem_lock(ow);
    ow_write(ow, 0xCC);
    ow_sem_unlock(ow);
}

bool ow_search(OneWire *const ow, uint8_t *new_addr, bool search_mode)
{
    ow_sem_lock(ow);
    uint8_t id_bit_number = 1;
    uint8_t last_zero = 0;
    uint8_t rom_byte_number = 0;
    bool search_result = false;
    uint8_t rom_byte_mask = 1;
    uint8_t search_direction;
    uint8_t id_bit, cmp_id_bit;

    if (!ow->LastDeviceFlag)
    {
        if (!ow_reset(ow))
        {
            ow->LastDiscrepancy = 0;
            ow->LastDeviceFlag = false;
            ow->LastFamilyDiscrepancy = 0;
            search_result = false;
            goto exit;
        }

        if (search_mode == true)
        {
            ow_write(ow, 0xF0);
        }
        else
        {
            ow_write(ow, 0xEC);
        }

        do
        {
            id_bit = ow_read_bit(ow);
            cmp_id_bit = ow_read_bit(ow);

            if ((id_bit == 1) && (cmp_id_bit == 1))
            {
                break;
            }
            else
            {
                if (id_bit != cmp_id_bit)
                {
                    search_direction = id_bit;
                }
                else
                {
                    if (id_bit_number < ow->LastDiscrepancy)
                    {
                        search_direction = ((ow->ROM_NO[rom_byte_number] & rom_byte_mask) > 0);
                    }
                    else
                    {
                        search_direction = (id_bit_number == ow->LastDiscrepancy);
                    }
                    if (search_direction == 0)
                    {
                        last_zero = id_bit_number;

                        if (last_zero < 9)
                        {
                            ow->LastFamilyDiscrepancy = last_zero;
                        }
                    }
                }

                if (search_direction == 1)
                {
                    ow->ROM_NO[rom_byte_number] |= rom_byte_mask;
                }
                else
                {
                    ow->ROM_NO[rom_byte_number] &= ~rom_byte_mask;
                }

                ow_write_bit(ow, search_direction);

                id_bit_number++;
                rom_byte_mask <<= 1;

                if (rom_byte_mask == 0)
                {
                    rom_byte_number++;
                    rom_byte_mask = 1;
                }
            }
        } while (rom_byte_number < 8);

        if (!(id_bit_number < 65))
        {
            ow->LastDiscrepancy = last_zero;

            if (ow->LastDiscrepancy == 0)
            {
                ow->LastDeviceFlag = true;
            }
            search_result = true;
        }
    }

    if (!search_result)
    {
        ow->LastDiscrepancy = 0;
        ow->LastDeviceFlag = false;
        ow->LastFamilyDiscrepancy = 0;
        search_result = false;
    }
    else
    {
        for (int i = 0; i < 8; i++)
        {
            new_addr[i] = ow->ROM_NO[i];
        }
    }

exit:
    ow_sem_unlock(ow);
    return search_result;
}

uint8_t ow_crc8(const uint8_t *addr, uint8_t len)
{
    uint8_t crc = 0;

    while (len--)
    {
        uint8_t inbyte = *addr++;
        for (uint8_t i = 8; i; i--)
        {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix)
            {
                crc ^= 0x8C;
            }
            inbyte >>= 1;
        }
    }
    return crc;
}

void ow_set_input(OneWire *ow)
{
    ow_sem_lock(ow);
    set_pin_input(ow->port, ow->pin);
    ow_sem_unlock(ow);
}

void ow_lock_bus(OneWire *ow)
{
    ow_sem_lock(ow);
}

void ow_unlock_bus(OneWire *ow)
{
    ow_sem_unlock(ow);
}
