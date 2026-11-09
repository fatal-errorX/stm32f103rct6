#ifndef BL_LOG_H
#define BL_LOG_H

#include <stdint.h>
#include "bl_config.h"

/*
 * 디버그 로그: USART1 TX(PA9) 로만 내보낸다 -> 보드의 CH340(USB-UART) COM 포트에서 115200 8N1 로 보인다.
 * USB CDC 가 안 떠도 부팅 진행 상황을 볼 수 있게 하기 위한 용도이며 수신은 하지 않는다.
 * bl_config.h 의 BL_LOG_ENABLE 을 0 으로 하면 코드가 전부 사라진다.
 */
#if BL_LOG_ENABLE

void bl_log_init(void);                    /* USART1 TX 준비. 여러 번 불러도 됨 */
void bl_log_deinit(void);                  /* 앱 점프 전: USART1/PA9 원상복구 */
void bl_log(const char *s);                /* "s\r\n" */
void bl_log_v(const char *s, uint32_t v);  /* "s=0x%08X\r\n" */
void bl_log_reset_cause(void);             /* RCC->CSR 의 리셋 원인을 출력하고 지움 */

#define BL_LOG_INIT()           bl_log_init()
#define BL_LOG_DEINIT()         bl_log_deinit()
#define BL_LOG(s)               bl_log(s)
#define BL_LOGV(s, v)           bl_log_v((s), (uint32_t)(v))
#define BL_LOG_RESET_CAUSE()    bl_log_reset_cause()

#else

#define BL_LOG_INIT()           ((void)0)
#define BL_LOG_DEINIT()         ((void)0)
#define BL_LOG(s)               ((void)0)
#define BL_LOGV(s, v)           ((void)0)
#define BL_LOG_RESET_CAUSE()    ((void)0)

#endif

#endif
