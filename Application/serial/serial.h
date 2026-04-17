#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>
#include "stm32c0xx_hal.h"

typedef enum
{
    SERIAL_OK = 0,
    SERIAL_ERR_TIMEOUT,
    SERIAL_ERR_BUSY,
    SERIAL_ERR_HAL
} Serial_err_t;

/**
 * @brief Initialize a UART peripheral for serial communication (8N1, no flow control).
 *
 * GPIO AF pin configuration is handled by HAL_UART_MspInit in
 * stm32c0xx_hal_msp.c (called automatically by HAL_UART_Init).
 *
 * @param instance  UART peripheral (e.g. USART1)
 * @param baud_rate Baud rate in bits per second
 */
void serial_init(USART_TypeDef *instance, uint32_t baud_rate);

/**
 * @brief Transmit bytes (blocking).
 */
Serial_err_t serial_write(const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Receive bytes (blocking).
 */
Serial_err_t serial_read(uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Printf-like formatted transmit. Truncates at 256 bytes.
 */
void serial_printf(const char *format, ...);

#endif /* SERIAL_H */
