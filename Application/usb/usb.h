#ifndef USB_H
#define USB_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize the USB module.
 */
void usb_init(void);

/**
 * @brief Periodic tasks to handle USB communication.
 * Should be called in the main loop.
 */
void usb_task(void);

/**
 * @brief Send raw data over the USB CDC interface.
 *
 * @param buffer Pointer to the data to send
 * @param size Number of bytes to send
 * @return uint32_t Number of bytes successfully sent
 */
uint32_t usb_write(const void *buffer, uint32_t size);

/**
 * @brief Read raw data from the USB CDC interface.
 *
 * @param buffer Pointer to the buffer to store received data
 * @param size Maximum number of bytes to read
 * @return uint32_t Number of bytes successfully read
 */
uint32_t usb_read(void *buffer, uint32_t size);

/**
 * @brief Formatted printf-like function for USB CDC output.
 *
 * @param format Printf formatting string
 * @param ... Arguments
 */
void usb_printf(const char *format, ...);

/**
 * @brief Check if a CDC terminal is connected.
 *
 * @return true if connected, false otherwise
 */
bool usb_connected(void);

#endif // USB_H
