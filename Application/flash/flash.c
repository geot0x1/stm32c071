#include "flash.h"
#include "stm32c0xx_hal.h"
#include <string.h>

#define FLASH_STORAGE_END_ADDR \
    (FLASH_STORAGE_START_ADDR + (FLASH_STORAGE_SECTOR_COUNT * FLASH_STORAGE_SECTOR_SIZE))

_Static_assert(FLASH_STORAGE_SECTOR_SIZE == FLASH_PAGE_SIZE,
    "FLASH_STORAGE_SECTOR_SIZE / FLASH_PAGE_SIZE mismatch");

static uint32_t addr_to_page(uint32_t addr)
{
    return (addr - FLASH_BASE) / FLASH_PAGE_SIZE;
}

bool flash_write(uint32_t addr, const void *data, uint16_t len)
{
    if (len == 0)
    {
        return true;
    }

    /* Validate: 8-byte alignment */
    if (addr & 0x7U)
    {
        return false;
    }

    /* Validate: range must lie entirely within the storage region */
    if (addr < FLASH_STORAGE_START_ADDR || addr + (uint32_t)len > FLASH_STORAGE_END_ADDR)
    {
        return false;
    }

    const uint8_t *src = (const uint8_t *)data;
    uint32_t dst = addr;
    uint16_t remaining = len;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return false;
    }

    while (remaining > 0)
    {
        uint8_t chunk[8];
        memset(chunk, 0xFF, sizeof(chunk));

        uint8_t n = (remaining >= 8U) ? 8U : (uint8_t)remaining;
        memcpy(chunk, src, n);

        uint64_t dword;
        memcpy(&dword, chunk, sizeof(dword));

        HAL_StatusTypeDef st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dst, dword);
        if (st != HAL_OK)
        {
            HAL_FLASH_Lock();
            return false;
        }
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP);

        src += n;
        dst += 8U;
        remaining -= n;
    }

    HAL_FLASH_Lock();
    return true;
}

bool flash_read(uint32_t addr, void *data, uint16_t len)
{
    if (addr < FLASH_STORAGE_START_ADDR || addr + (uint32_t)len > FLASH_STORAGE_END_ADDR)
    {
        return false;
    }
    memcpy(data, (const void *)addr, len);
    return true;
}

bool flash_erase_page(uint32_t addr)
{
    /* Validate: addr must fall within the storage region */
    if (addr < FLASH_STORAGE_START_ADDR || addr >= FLASH_STORAGE_END_ADDR)
    {
        return false;
    }

    FLASH_EraseInitTypeDef erase = {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Page = addr_to_page(addr),
        .NbPages = 1U,
    };

    uint32_t page_error = 0;

    if (HAL_FLASH_Unlock() != HAL_OK)
    {
        return false;
    }
    HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();

    return (st == HAL_OK) && (page_error == 0xFFFFFFFFU);
}
