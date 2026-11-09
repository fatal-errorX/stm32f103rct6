#ifndef HAL_H
#define HAL_H

#include <stdint.h>

void     systick_init(void);
void     systick_deinit(void);
uint32_t millis(void);

void uart_init(uint32_t baud);
void uart_deinit(void);
int  uart_getc(uint32_t timeout_ms);   /* 0..255, 타임아웃이면 -1 */
void uart_putc(uint8_t c);
void uart_write(const uint8_t *buf, uint32_t len);
void uart_flush(void);

void flash_unlock(void);
void flash_lock(void);
int  flash_erase_page(uint32_t addr);               /* 0: OK */
int  flash_write_half(uint32_t addr, uint16_t val); /* 0: OK */

int  bkp_take_magic(uint16_t magic);  /* DR1==magic 이면 지우고 1 반환 */

#endif
