#include "onewire.h"
#include "critical.h"
#include "delay.h"

#define ENTER_CRITICAL() critical_enter()
#define EXIT_CRITICAL()  critical_exit()

#define TIME_DELAY_A (4)
#define TIME_DELAY_B (64)
#define TIME_DELAY_C (80)
#define TIME_DELAY_D (14)
#define TIME_DELAY_E (14)
#define TIME_DELAY_F (45)
#define TIME_DELAY_G (0)
#define TIME_DELAY_H (480)
#define TIME_DELAY_I (55)
#define TIME_DELAY_J (414)

static void ow_sem_lock(OneWire* ow);
static void ow_sem_unlock(OneWire* ow);
static void ow_write_bit(OneWire *const ow, uint8_t v);

static void ow_enable_gpio_clock(GPIO_TypeDef* port)
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

static inline void __attribute__((optimize("O0"))) set_pin_output(GPIO_TypeDef* port, const uint16_t pin, bool level)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
    HAL_GPIO_WritePin(port, pin, level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static inline void __attribute__((optimize("O0"))) set_pin_input(GPIO_TypeDef* port, const uint16_t pin)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(port, &GPIO_InitStruct);
}

static inline bool __attribute__((optimize("O0"))) read_pin(GPIO_TypeDef* port, const uint16_t pin)
{
    bool level = false;
    volatile uint_fast8_t low_cntr  = 0;
    volatile uint_fast8_t high_cntr = 0;
    for (uint_fast8_t i = 0; i < 32; i++)
    {
        if (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_SET)
        {
            low_cntr = 0;
            high_cntr++;
        }
        else
        {
            high_cntr = 0;
            low_cntr++;
        }
        if (high_cntr > 5)
        {
            level = true;
            break;
        }
        if (low_cntr > 5)
        {
            level = false;
            break;
        }
    }
    return level;
}

void ow_set_low(OneWire* ow)
{
    ow_sem_lock(ow);
    set_pin_output(ow->port, ow->pin, false);
    ow_sem_unlock(ow);
}

void ow_init(OneWire* ow)
{
    ow->lock_count = 0;
    ow_enable_gpio_clock(ow->port);
    ow_sem_lock(ow);
    ow_set_input(ow);
    ow_sem_unlock(ow);
}

static void ow_sem_lock(OneWire* ow)
{
    ENTER_CRITICAL();
    ow->lock_count++;
    EXIT_CRITICAL();
}

static void ow_sem_unlock(OneWire* ow)
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

    set_pin_output(ow->port, ow->pin, false);

    delay_us(TIME_DELAY_H);

    set_pin_input(ow->port, ow->pin);
    delay_us(TIME_DELAY_I); 
    
    err = !read_pin(ow->port, ow->pin); 

    delay_us(TIME_DELAY_J); 

    EXIT_CRITICAL();

    ow_sem_unlock(ow);

    return err;
}

void ow_reset_search(OneWire *const ow)
{
    ow->LastDiscrepancy       = 0;
    ow->LastDeviceFlag        = false;
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
        set_pin_output(ow->port, ow->pin, false);
        delay_us(TIME_DELAY_A);

        set_pin_output(ow->port, ow->pin, true);
        delay_us(TIME_DELAY_B);
        EXIT_CRITICAL();
    }
    else
    {
        ENTER_CRITICAL();
        set_pin_output(ow->port, ow->pin, false);
        delay_us(TIME_DELAY_C);

        set_pin_output(ow->port, ow->pin, true);
        delay_us(TIME_DELAY_D); 
        EXIT_CRITICAL();
    }
    ow_sem_unlock(ow);
}

uint8_t ow_pin_status(OneWire *const ow)
{
    ow_sem_lock(ow);
    set_pin_input(ow->port, ow->pin);
    uint8_t status = read_pin(ow->port, ow->pin);
    ow_sem_unlock(ow);
    
    return status ? 1 : 0;
}

uint8_t ow_read_bit(OneWire *const ow)
{
    ow_sem_lock(ow);

    uint8_t r;

    ENTER_CRITICAL();

    set_pin_output(ow->port, ow->pin, false);
    delay_us(TIME_DELAY_A);

    set_pin_input(ow->port, ow->pin);
    delay_us(TIME_DELAY_E);

    r = read_pin(ow->port, ow->pin);

    EXIT_CRITICAL();

    ow_sem_unlock(ow);
    return r;
}

void ow_write(OneWire *const ow, uint8_t v)
{
    ow_sem_lock(ow);
    for (uint8_t bitMask = 0x01; bitMask; bitMask <<= 1)
    {
        ow_write_bit(ow, (bitMask & v) ? 1 : 0);
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
    uint8_t bitMask;
    uint8_t r = 0;
    ow_sem_lock(ow);
    for (bitMask = 0x01; bitMask; bitMask <<= 1)
    {
        while (!ow_pin_status(ow))
        {
            delay_us(5);
        }
        if (ow_read_bit(ow))
        {
            r |= bitMask;
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
    uint8_t id_bit_number   = 1;
    uint8_t last_zero       = 0;
    uint8_t rom_byte_number = 0;
    bool    search_result   = false;
    uint8_t rom_byte_mask   = 1;
    uint8_t search_direction;
    uint8_t id_bit, cmp_id_bit;

    if (!ow->LastDeviceFlag)
    {       
        if (!ow_reset(ow))
        {
            ow->LastDiscrepancy       = 0;
            ow->LastDeviceFlag        = false;
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

        delay_us(100);

        do
        {
            id_bit     = ow_read_bit(ow);
            delay_us(100);
            cmp_id_bit = ow_read_bit(ow);
            delay_us(100);

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

    if (!search_result || !ow->ROM_NO[0])
    {
        ow->LastDiscrepancy       = 0;
        ow->LastDeviceFlag        = false;
        ow->LastFamilyDiscrepancy = 0;
        search_result             = false;
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

void ow_set_input(OneWire* ow)
{
    ow_sem_lock(ow);
    set_pin_input(ow->port, ow->pin);
    ow_sem_unlock(ow);
}

void ow_lock_bus(OneWire* ow)
{
    ow_sem_lock(ow);
}

void ow_unlock_bus(OneWire* ow)
{
    ow_sem_unlock(ow);
}
