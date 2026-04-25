#include "flash.h"
#include "stm32c0xx_hal.h"
#include <string.h>

static uint32_t addr_to_page(uint32_t addr)
{
    return (addr - FLASH_BASE) / FLASH_PAGE_SIZE;
}

void flash_write(uint32_t addr, const void *data, uint16_t len)
{
    const uint8_t *src = (const uint8_t *)data;
    uint32_t dst = addr;
    uint16_t remaining = len;

    HAL_FLASH_Unlock();

    while (remaining > 0)
    {
        uint8_t chunk[8];
        memset(chunk, 0xFF, sizeof(chunk));

        uint8_t n = (remaining >= 8U) ? 8U : (uint8_t)remaining;
        memcpy(chunk, src, n);

        uint64_t dword;
        memcpy(&dword, chunk, sizeof(dword));

        HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dst, dword);

        src += n;
        dst += 8U;
        remaining -= n;
    }

    HAL_FLASH_Lock();
}

void flash_read(uint32_t addr, void *data, uint16_t len)
{
    memcpy(data, (const void *)addr, len);
}

void flash_erase_page(uint32_t addr)
{
    FLASH_EraseInitTypeDef erase =
    {
        .TypeErase = FLASH_TYPEERASE_PAGES,
        .Page      = addr_to_page(addr),
        .NbPages   = 1U,
    };

    uint32_t page_error;

    HAL_FLASH_Unlock();
    HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();
}
