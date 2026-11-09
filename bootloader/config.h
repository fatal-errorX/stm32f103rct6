#ifndef CONFIG_H
#define CONFIG_H

/* ---- STM32F103RCT6: 256KB Flash, 48KB RAM, 2KB page ---- */
#define FLASH_BASE_ADDR     0x08000000UL
#define FLASH_TOTAL_SIZE    (256UL * 1024UL)
#define FLASH_PAGE_SIZE     2048UL

#define RAM_BASE_ADDR       0x20000000UL
#define RAM_SIZE            (48UL * 1024UL)

/* 부트로더 16KB, 애플리케이션은 0x08004000 부터 240KB */
#define BL_SIZE             (16UL * 1024UL)
#define APP_START_ADDR      (FLASH_BASE_ADDR + BL_SIZE)
#define APP_MAX_SIZE        (FLASH_TOTAL_SIZE - BL_SIZE)

/* 클럭: 리셋 기본값인 HSI 8MHz 그대로 사용 (앱으로 넘어갈 때 정리할 게 없음) */
#define F_CPU               8000000UL

/* USART1: PA9(TX) / PA10(RX) */
#define BL_BAUDRATE         115200UL

/* 리셋 후 호스트의 PING 을 기다리는 시간 */
#define BOOT_WAIT_MS        500UL
/* 패킷 내 바이트 간 타임아웃 */
#define BYTE_TIMEOUT_MS     100UL

/* 한 번의 WRITE 패킷에 담을 수 있는 최대 데이터 크기 */
#define MAX_CHUNK           1024U

/* 앱에서 BKP->DR1 에 이 값을 쓰고 리셋하면 부트로더에 머무름 */
#define BL_MAGIC            0xB007U

#define BL_VERSION_MAJOR    1
#define BL_VERSION_MINOR    0

#endif
