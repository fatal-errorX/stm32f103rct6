#ifndef REGS_H
#define REGS_H

/* CMSIS 없이 빌드되도록 필요한 레지스터만 직접 정의 (RM0008 기준) */
#include <stdint.h>

#define REG32(addr)         (*(volatile uint32_t *)(addr))

/* RCC */
#define RCC_BASE            0x40021000UL
#define RCC_APB2RSTR        REG32(RCC_BASE + 0x0C)
#define RCC_APB2ENR         REG32(RCC_BASE + 0x18)
#define RCC_APB1ENR         REG32(RCC_BASE + 0x1C)
#define RCC_APB2_AFIO       (1UL << 0)
#define RCC_APB2_IOPA       (1UL << 2)
#define RCC_APB2_USART1     (1UL << 14)
#define RCC_APB1_BKP        (1UL << 27)
#define RCC_APB1_PWR        (1UL << 28)

/* GPIOA */
#define GPIOA_BASE          0x40010800UL
#define GPIOA_CRH           REG32(GPIOA_BASE + 0x04)
#define GPIOA_ODR           REG32(GPIOA_BASE + 0x0C)

/* USART1 */
#define USART1_BASE         0x40013800UL
#define USART1_SR           REG32(USART1_BASE + 0x00)
#define USART1_DR           REG32(USART1_BASE + 0x04)
#define USART1_BRR          REG32(USART1_BASE + 0x08)
#define USART1_CR1          REG32(USART1_BASE + 0x0C)
#define USART_SR_RXNE       (1UL << 5)
#define USART_SR_TC         (1UL << 6)
#define USART_SR_TXE        (1UL << 7)
#define USART_CR1_RE        (1UL << 2)
#define USART_CR1_TE        (1UL << 3)
#define USART_CR1_UE        (1UL << 13)

/* FLASH interface */
#define FLASH_R_BASE        0x40022000UL
#define FLASH_KEYR          REG32(FLASH_R_BASE + 0x04)
#define FLASH_SR            REG32(FLASH_R_BASE + 0x0C)
#define FLASH_CR            REG32(FLASH_R_BASE + 0x10)
#define FLASH_AR            REG32(FLASH_R_BASE + 0x14)
#define FLASH_KEY1          0x45670123UL
#define FLASH_KEY2          0xCDEF89ABUL
#define FLASH_SR_BSY        (1UL << 0)
#define FLASH_SR_PGERR      (1UL << 2)
#define FLASH_SR_WRPRTERR   (1UL << 4)
#define FLASH_SR_EOP        (1UL << 5)
#define FLASH_CR_PG         (1UL << 0)
#define FLASH_CR_PER        (1UL << 1)
#define FLASH_CR_STRT       (1UL << 6)
#define FLASH_CR_LOCK       (1UL << 7)

/* PWR / BKP */
#define PWR_CR              REG32(0x40007000UL)
#define PWR_CR_DBP          (1UL << 8)
#define BKP_DR1             REG32(0x40006C04UL)

/* Cortex-M3 core */
#define SYST_CSR            REG32(0xE000E010UL)
#define SYST_RVR            REG32(0xE000E014UL)
#define SYST_CVR            REG32(0xE000E018UL)
#define SCB_ICSR            REG32(0xE000ED04UL)
#define SCB_VTOR            REG32(0xE000ED08UL)
#define SCB_AIRCR           REG32(0xE000ED0CUL)
#define SCB_ICSR_PENDSTCLR  (1UL << 25)

#endif
