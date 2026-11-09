#include "main.h"
#include "bl_log.h"

#if BL_LOG_ENABLE

static uint8_t s_ready;

void bl_log_init(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART1_CLK_ENABLE();

    g.Pin   = GPIO_PIN_9;                   /* USART1_TX */
    g.Mode  = GPIO_MODE_AF_PP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
    s_ready = 1;
}

void bl_log_deinit(void)
{
    if (!s_ready)
        return;
    while (!(USART1->SR & USART_SR_TC))
        ;
    USART1->CR1 = 0;
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9);
    __HAL_RCC_USART1_FORCE_RESET();
    __HAL_RCC_USART1_RELEASE_RESET();
    __HAL_RCC_USART1_CLK_DISABLE();
    s_ready = 0;
}

static void putc_raw(char c)
{
    while (!(USART1->SR & USART_SR_TXE))
        ;
    USART1->DR = (uint8_t)c;
}

static void puts_raw(const char *s)
{
    /* 클럭이 중간에 바뀌므로(HSI 8MHz -> PLL 72MHz) 출력할 때마다 분주비를 다시 계산 */
    USART1->BRR = (HAL_RCC_GetPCLK2Freq() + BL_LOG_BAUD / 2U) / BL_LOG_BAUD;
    while (*s)
        putc_raw(*s++);
}

void bl_log(const char *s)
{
    if (!s_ready)
        bl_log_init();
    puts_raw(s);
    puts_raw("\r\n");
}

void bl_log_v(const char *s, uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[12] = "=0x";

    for (int i = 0; i < 8; i++)
        buf[3 + i] = hex[(v >> (28 - 4 * i)) & 0xFU];
    buf[11] = '\0';

    if (!s_ready)
        bl_log_init();
    puts_raw(s);
    puts_raw(buf);
    puts_raw("\r\n");
}

void bl_log_reset_cause(void)
{
    uint32_t csr = RCC->CSR;

    bl_log_v("[BL] reset CSR", csr);
    if (csr & RCC_CSR_PORRSTF)  bl_log("[BL]   POR (power on)");
    if (csr & RCC_CSR_PINRSTF)  bl_log("[BL]   PIN (reset button)");
    if (csr & RCC_CSR_SFTRSTF)  bl_log("[BL]   SOFTWARE (NVIC_SystemReset)");
    if (csr & RCC_CSR_IWDGRSTF) bl_log("[BL]   IWDG watchdog");
    if (csr & RCC_CSR_WWDGRSTF) bl_log("[BL]   WWDG watchdog");
    if (csr & RCC_CSR_LPWRRSTF) bl_log("[BL]   low-power");
    RCC->CSR |= RCC_CSR_RMVF;
}

#endif /* BL_LOG_ENABLE */
