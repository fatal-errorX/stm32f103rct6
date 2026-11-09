#ifndef BL_IO_H
#define BL_IO_H

#include <stdint.h>

/*
 * 전송 계층 인터페이스. bl_core.c 는 이 함수들만 사용하므로
 * USB CDC(bl_io_usb.c) 대신 UART 구현 등으로 파일 하나만 바꿔 끼울 수 있다.
 */

/* 전송 계층을 켜기 "전"에 부르는 준비 (HAL_Init/클럭 설정 후, MX_USB_DEVICE_Init 전).
 * USB 는 여기서 D+ 를 잠깐 눌러 호스트가 항상 새 장치로 인식하게 한다 */
void bl_io_prepare(void);
/* 전송 계층 준비 (CubeMX 의 초기화 함수들이 끝난 뒤 호출) */
void bl_io_init(void);
/* 1바이트 수신. timeout_ms 안에 없으면 -1 */
int  bl_io_getc(uint32_t timeout_ms);
/* 송신 (블로킹). 연결이 없으면 버린다 */
void bl_io_write(const uint8_t *buf, uint32_t len);
/* 송신이 모두 나갈 때까지 대기 */
void bl_io_flush(void);
/* 전송 계층 상태를 디버그 로그로 출력 (연결 상태, 수신 바이트 수 등) */
void bl_io_log_status(void);
/* 앱으로 점프하기 전에 주변장치 정리 */
void bl_io_deinit(void);

#endif
