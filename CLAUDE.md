# STM32F103RCT6 UART 부트로더

## 구조
- bootloader/ : 부트로더 펌웨어 (레지스터 직접 제어, CMSIS/HAL 미사용), arm-none-eabi-gcc + make
- host/uart_flash.py : CLI 업로더, upload() 함수에 다운로드 절차가 있음 (GUI와 공유)
- host/uart_flash_gui.py : tkinter GUI, 작업 스레드 + queue 로 UI 갱신

## 핵심 설계
- 메모리: 부트로더 0x08000000 (16KB), 앱 0x08004000 (240KB), 페이지 2KB
- USART1 PA9/PA10, 115200, HSI 8MHz (PLL 안 씀)
- 부팅: BKP->DR1==0xB007 이거나 앱 무효면 머묾, 아니면 500ms 동안 PING 대기 후 점프
- 앱 첫 8바이트(SP, Reset 벡터)는 RAM에 보관 → VERIFY 의 CRC32 통과 후 마지막에 기록 (전원 차단 대비)
- 프로토콜: A5|CMD|LEN|PAYLOAD|CRC16, 응답 A5|CMD|STATUS|LEN|DATA|CRC16 (README 참고)
- 같은 값 재기록은 건너뜀 (재전송 시 PGERR 방지)

## 빌드
- 툴체인: STM32CubeIDE 내장 GCC (C:/ST/STM32CubeIDE_2.1.1/.../gnu-tools-for-stm32.../tools/bin) 를 PATH 에 추가 후 `make` (bootloader/ 에서)
- 호스트: `pip install pyserial` (tkinter 는 기본 포함)

## 상태
- 빌드 확인됨 (경고 없음, 약 2KB), C/Python CRC 일치 확인
- 가짜 부트로더 시뮬레이션으로 호스트 업로드/재시도/취소 확인
- 실제 보드 테스트는 아직 안 함

## 규칙
- 펌웨어 프로토콜을 바꾸면 host/uart_flash.py 도 같이 수정
- 응답과 주석은 한국어
