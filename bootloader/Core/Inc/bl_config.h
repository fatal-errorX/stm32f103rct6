#ifndef BL_CONFIG_H
#define BL_CONFIG_H

/*
 * 부트로더 설정 - 메모리 배치/타이밍/프로토콜 상수는 모두 여기서만 바꾼다.
 * (CubeMX 생성 파일과 무관하므로 재생성해도 유지됨)
 */

/* ---- STM32F103RCT6: 256KB Flash, 48KB RAM, 2KB page ---- */
#define FLASH_BASE_ADDR     0x08000000UL
#define FLASH_TOTAL_SIZE    (256UL * 1024UL)
#define FLASH_PAGE_SIZE_B   2048UL

#define RAM_BASE_ADDR       0x20000000UL
#define RAM_SIZE_B          (48UL * 1024UL)

/*
 * 부트로더 크기. 바꾸면 링커 스크립트(STM32F103RCTX_FLASH.ld 의 FLASH LENGTH),
 * 앱 링커 시작 주소, 앱 system_stm32f1xx.c 의 VECT_TAB_OFFSET 도 같이 맞춰야 한다.
 * 페이지(2KB) 배수여야 한다.
 */
#define BL_SIZE             (32UL * 1024UL)
#define APP_START_ADDR      (FLASH_BASE_ADDR + BL_SIZE)
#define APP_MAX_SIZE        (FLASH_TOTAL_SIZE - BL_SIZE)

/*
 * 리셋 후 호스트의 PING 을 기다리는 시간.
 * USB CDC 는 열거(enumeration)와 호스트의 포트 재오픈에 1~2초 걸리므로 UART 때보다 길게 잡는다.
 */
#define BOOT_WAIT_MS        3000UL
/* 패킷 내 바이트 간 타임아웃 */
#define BYTE_TIMEOUT_MS     100UL

/* 한 번의 WRITE 패킷에 담을 수 있는 최대 데이터 크기 */
#define MAX_CHUNK           1024U

/* 앱에서 BKP->DR1 에 이 값을 쓰고 리셋하면 부트로더에 머무름 */
#define BL_MAGIC            0xB007U

#define BL_VERSION_MAJOR    1
#define BL_VERSION_MINOR    2   /* 1.2 = HAL + USB CDC (프로토콜은 1.0 과 동일) */

/* 디버그 로그 (USART1 TX = PA9 -> CH340 COM 포트, 115200). 0 이면 전부 제거 */
#define BL_LOG_ENABLE       1
#define BL_LOG_BAUD         115200UL

/* 부트로더 시작/앱 점프 때 D+ 를 내려 호스트가 재열거하도록 하는 시간 */
#define USB_DETACH_MS       50U

#endif
