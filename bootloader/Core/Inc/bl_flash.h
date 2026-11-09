#ifndef BL_FLASH_H
#define BL_FLASH_H

#include <stdint.h>

/* 모두 성공 0, 실패 -1. 호출 전 HAL_FLASH_Unlock(), 후 HAL_FLASH_Lock() 은 호출한 쪽 책임 */
int bl_flash_erase_page(uint32_t addr);
/* 이미 같은 값이면 건너뜀 (재전송 시 PGERR 방지) */
int bl_flash_write_half(uint32_t addr, uint16_t val);

#endif
