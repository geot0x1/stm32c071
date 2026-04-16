#include "uart.h"
#include <string.h>

static Uart_err_t hal_to_uart_err(HAL_StatusTypeDef status)
{
    switch (status)
    {
        case HAL_OK:      return UART_OK;
        case HAL_TIMEOUT: return UART_ERR_TIMEOUT;
        case HAL_BUSY:    return UART_ERR_BUSY;
        default:          return UART_ERR_HAL;
    }
}

void uart_init(Uart_t *uart, USART_TypeDef *instance, uint32_t baud_rate)
{
    memset(uart, 0, sizeof(Uart_t));

    uart->hal_handle.Instance            = instance;
    uart->hal_handle.Init.BaudRate       = baud_rate;
    uart->hal_handle.Init.WordLength     = UART_WORDLENGTH_8B;
    uart->hal_handle.Init.StopBits       = UART_STOPBITS_1;
    uart->hal_handle.Init.Parity         = UART_PARITY_NONE;
    uart->hal_handle.Init.Mode           = UART_MODE_TX_RX;
    uart->hal_handle.Init.HwFlowCtl     = UART_HWCONTROL_NONE;
    uart->hal_handle.Init.OverSampling   = UART_OVERSAMPLING_16;
    uart->hal_handle.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    uart->hal_handle.Init.ClockPrescaler = UART_PRESCALER_DIV1;

    HAL_UART_Init(&uart->hal_handle);
}

Uart_err_t uart_write(Uart_t *uart, const uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return hal_to_uart_err(
        HAL_UART_Transmit(&uart->hal_handle, (uint8_t *)data, len, timeout_ms));
}

Uart_err_t uart_read(Uart_t *uart, uint8_t *data, uint16_t len, uint32_t timeout_ms)
{
    return hal_to_uart_err(
        HAL_UART_Receive(&uart->hal_handle, data, len, timeout_ms));
}
