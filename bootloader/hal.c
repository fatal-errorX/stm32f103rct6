#include "hal.h"
#include "regs.h"
#include "config.h"

/* ---------------- SysTick (1ms) ---------------- */
static volatile uint32_t s_ticks;

void SysTick_Handler(void)
{
    s_ticks++;
}

void systick_init(void)
{
    SYST_RVR = (F_CPU / 1000UL) - 1UL;
    SYST_CVR = 0;
    SYST_CSR = 0x7;   /* CLKSOURCE=core, TICKINT, ENABLE */
}

void systick_deinit(void)
{
    SYST_CSR = 0;
    SYST_CVR = 0;
    SCB_ICSR = SCB_ICSR_PENDSTCLR;
}

uint32_t millis(void)
{
    return s_ticks;
}

/* ---------------- USART1 (polling) ---------------- */
void uart_init(uint32_t baud)
{
    RCC_APB2ENR |= RCC_APB2_IOPA | RCC_APB2_AFIO | RCC_APB2_USART1;

    /* PA9: AF push-pull 50MHz (0xB), PA10: input pull-up (0x8) */
    uint32_t crh = GPIOA_CRH;
    crh &= ~((0xFUL << 4) | (0xFUL << 8));
    crh |=  (0xBUL << 4) | (0x8UL << 8);
    GPIOA_CRH = crh;
    GPIOA_ODR |= (1UL << 10);

    USART1_BRR = (F_CPU + baud / 2U) / baud;
    USART1_CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

void uart_deinit(void)
{
    uint32_t mask = RCC_APB2_IOPA | RCC_APB2_AFIO | RCC_APB2_USART1;
    RCC_APB2RSTR |= mask;
    RCC_APB2RSTR &= ~mask;
    RCC_APB2ENR  &= ~mask;
}

int uart_getc(uint32_t timeout_ms)
{
    uint32_t start = millis();
    while (!(USART1_SR & USART_SR_RXNE)) {
        if ((millis() - start) >= timeout_ms)
            return -1;
    }
    /* SR 읽고 DR 읽으면 ORE 도 같이 클리어됨 */
    return (int)(USART1_DR & 0xFFU);
}

void uart_putc(uint8_t c)
{
    while (!(USART1_SR & USART_SR_TXE))
        ;
    USART1_DR = c;
}

void uart_write(const uint8_t *buf, uint32_t len)
{
    while (len--)
        uart_putc(*buf++);
}

void uart_flush(void)
{
    while (!(USART1_SR & USART_SR_TC))
        ;
}

/* ---------------- Flash ---------------- */
void flash_unlock(void)
{
    if (FLASH_CR & FLASH_CR_LOCK) {
        FLASH_KEYR = FLASH_KEY1;
        FLASH_KEYR = FLASH_KEY2;
    }
}

void flash_lock(void)
{
    FLASH_CR |= FLASH_CR_LOCK;
}

static int flash_wait(void)
{
    while (FLASH_SR & FLASH_SR_BSY)
        ;
    uint32_t sr = FLASH_SR;
    FLASH_SR = FLASH_SR_EOP | FLASH_SR_PGERR | FLASH_SR_WRPRTERR;  /* write-1-to-clear */
    return (sr & (FLASH_SR_PGERR | FLASH_SR_WRPRTERR)) ? -1 : 0;
}

int flash_erase_page(uint32_t addr)
{
    flash_wait();
    FLASH_CR |= FLASH_CR_PER;
    FLASH_AR = addr;
    FLASH_CR |= FLASH_CR_STRT;
    int r = flash_wait();
    FLASH_CR &= ~FLASH_CR_PER;
    if (r)
        return r;

    for (uint32_t i = 0; i < FLASH_PAGE_SIZE; i += 4) {
        if (REG32(addr + i) != 0xFFFFFFFFUL)
            return -1;
    }
    return 0;
}

int flash_write_half(uint32_t addr, uint16_t val)
{
    /* 이미 같은 값이면 건너뜀: 응답 유실로 호스트가 같은 청크를 재전송해도 PGERR 안 남 */
    if (*(volatile uint16_t *)addr == val)
        return 0;

    flash_wait();
    FLASH_CR |= FLASH_CR_PG;
    *(volatile uint16_t *)addr = val;
    int r = flash_wait();
    FLASH_CR &= ~FLASH_CR_PG;
    if (r)
        return r;
    return (*(volatile uint16_t *)addr == val) ? 0 : -1;
}

/* ---------------- Backup register ---------------- */
int bkp_take_magic(uint16_t magic)
{
    int hit = 0;

    RCC_APB1ENR |= RCC_APB1_PWR | RCC_APB1_BKP;
    if ((BKP_DR1 & 0xFFFFU) == magic) {
        PWR_CR |= PWR_CR_DBP;
        BKP_DR1 = 0;
        PWR_CR &= ~PWR_CR_DBP;
        hit = 1;
    }
    RCC_APB1ENR &= ~(RCC_APB1_PWR | RCC_APB1_BKP);
    return hit;
}
