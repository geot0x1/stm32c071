#ifndef FLASH_H
#define FLASH_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Storage region: last FLASH_STORAGE_SECTOR_COUNT pages of flash.
 * Each page is 2 KB (hardware erase granularity).
 *
 * Layout (128 KB device, 64 pages):
 *   Pages 0–59  : firmware
 *   Pages 60–63 : storage (8 KB)
 */
#define FLASH_STORAGE_SECTOR_SIZE 2048U
#define FLASH_STORAGE_SECTOR_COUNT 4U
#define FLASH_STORAGE_START_ADDR \
    (0x08020000U - (FLASH_STORAGE_SECTOR_COUNT * FLASH_STORAGE_SECTOR_SIZE))

/**
 * @brief Write bytes to internal flash.
 *
 * The target range must already be erased (0xFF).
 * Writes are padded to the 8-byte hardware granularity with 0xFF.
 *
 * @param addr  Absolute flash address (must be 8-byte aligned).
 * @param data  Source buffer.
 * @param len   Number of bytes to write.
 * @return true on success, false if addr is misaligned, out of storage range, or a HAL error
 * occurs.
 */
bool flash_write(uint32_t addr, const void *data, uint16_t len);

/**
 * @brief Read bytes from internal flash (direct memory-mapped copy).
 *
 * @param addr  Absolute flash address (must be within storage region).
 * @param data  Destination buffer.
 * @param len   Number of bytes to read.
 * @return true on success, false if the range falls outside the storage region.
 */
bool flash_read(uint32_t addr, void *data, uint16_t len);

/**
 * @brief Erase the 2 KB page that contains the given address.
 *
 * @param addr  Any address within the target page (must be within storage region).
 * @return true on success, false if addr is out of storage range or a HAL error occurs.
 */
bool flash_erase_page(uint32_t addr);

#endif /* FLASH_H */
