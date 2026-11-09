#include "main.h"
#include "bl_flash.h"
#include "bl_config.h"

int bl_flash_erase_page(uint32_t addr)
{
    FLASH_EraseInitTypeDef e = {
        .TypeErase   = FLASH_TYPEERASE_PAGES,
        .PageAddress = addr,
        .NbPages     = 1,
    };
    uint32_t page_err = 0;

    if (HAL_FLASHEx_Erase(&e, &page_err) != HAL_OK)
        return -1;

    for (uint32_t i = 0; i < FLASH_PAGE_SIZE_B; i += 4) {
        if (*(volatile uint32_t *)(addr + i) != 0xFFFFFFFFUL)
            return -1;
    }
    return 0;
}

int bl_flash_write_half(uint32_t addr, uint16_t val)
{
    if (*(volatile uint16_t *)addr == val)
        return 0;

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, val) != HAL_OK)
        return -1;
    return (*(volatile uint16_t *)addr == val) ? 0 : -1;
}
