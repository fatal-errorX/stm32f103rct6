#include <stdint.h>

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
extern int main(void);
extern void SysTick_Handler(void);

void Reset_Handler(void)
{
    uint32_t *src = &_sidata, *dst = &_sdata;
    while (dst < &_edata)
        *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss; )
        *dst++ = 0;

    main();
    for (;;)
        ;
}

static void Default_Handler(void)
{
    /* 부트로더에서 폴트가 나면 리셋해서 다시 시도 */
    *(volatile uint32_t *)0xE000ED0CUL = 0x05FA0004UL;
    for (;;)
        ;
}

/* 부트로더는 SysTick 외 인터럽트를 쓰지 않으므로 코어 예외 16개만 둔다 */
__attribute__((section(".isr_vector"), used))
static void (* const vectors[16])(void) = {
    (void (*)(void))&_estack,
    Reset_Handler,
    Default_Handler,    /* NMI */
    Default_Handler,    /* HardFault */
    Default_Handler,    /* MemManage */
    Default_Handler,    /* BusFault */
    Default_Handler,    /* UsageFault */
    0, 0, 0, 0,
    Default_Handler,    /* SVCall */
    Default_Handler,    /* DebugMon */
    0,
    Default_Handler,    /* PendSV */
    SysTick_Handler,
};
