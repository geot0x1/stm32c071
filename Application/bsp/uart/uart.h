#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include "stm32c0xx_hal.h"

typedef struct Uart_s
{
    UART_HandleTypeDef hal_handle;
} Uart_t;

typedef enum
{
    UART_OK = 0,
    UART_ERR_TIMEOUT,
    UART_ERR_BUSY,
    UART_ERR_HAL
} Uart_err_t;

/**
 * @brief Initialize a UART peripheral (8N1, no flow control).
 *
 * GPIO AF pin configuration is handled by HAL_UART_MspInit in
 * stm32c0xx_hal_msp.c (called automatically by HAL_UART_Init).
 *
 * @param uart      Handle to fill
 * @param instance  UART peripheral (USART1, USART2, …)
 * @param baud_rate Baud rate in bits per second
 */
void uart_init(Uart_t *uart, USART_TypeDef *instance, uint32_t baud_rate);

/**
 * @brief Transmit bytes (blocking).
 */
Uart_err_t uart_write(Uart_t *uart, const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Receive bytes (blocking).
 */
Uart_err_t uart_read(Uart_t *uart, uint8_t *data, uint16_t len, uint32_t timeout_ms);

#endif /* BSP_UART_H */
