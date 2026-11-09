# STM32F103RCT6 USB CDC 부트로더

## 구조
- bootloader/ : CubeMX(HAL + USB Device CDC) 프로젝트, STM32CubeIDE 로 빌드. 직접 작성한 로직은 Core/Src/bl_*.c, Core/Inc/bl_*.h, crc.c 에만 있고 CubeMX 생성 파일은 `USER CODE` 영역 밖을 수정하지 않는다 (예외: 링커 스크립트 FLASH LENGTH)
- host/uart_flash.py : CLI 업로더, upload() 함수에 다운로드 절차가 있음 (GUI와 공유)
- host/uart_flash_gui.py : tkinter GUI, 작업 스레드 + queue 로 UI 갱신

## 핵심 설계
- 메모리: 부트로더 0x08000000 (32KB), 앱 0x08008000 (224KB), 페이지 2KB (BL_SIZE 는 bl_config.h + 링커 스크립트에서 같이 관리)
- USB CDC (PA11/PA12, VID:PID 0483:5740), HSE 8MHz → PLL 72MHz. 전송 계층은 bl_io.h 뒤에 분리 (UART 로 교체 가능)
- 부팅: BKP->DR1==0xB007 이거나 앱 무효면 머묾, 아니면 3초(BOOT_WAIT_MS) 동안 PING 대기 후 점프. 점프 직전 D+ 를 눌러 호스트가 재열거하게 함 (보드의 D+ 풀업이 고정이라서)
- 앱 첫 8바이트(SP, Reset 벡터)는 RAM에 보관 → VERIFY 의 CRC32 통과 후 마지막에 기록 (전원 차단 대비)
- 프로토콜: A5|CMD|LEN|PAYLOAD|CRC16, 응답 A5|CMD|STATUS|LEN|DATA|CRC16 (README 참고)
- 같은 값 재기록은 건너뜀 (재전송 시 PGERR 방지)

## 빌드
- 부트로더: STM32CubeIDE 에서 빌드. -Os 에서 약 17KB 라 Debug(-O0, 약 36KB)는 32KB 를 넘으므로 Optimization -Os 로 설정
- 빌드 검증용으로 CubeIDE 내장 GCC (C:/ST/STM32CubeIDE_2.1.1/.../gnu-tools-for-stm32.../tools/bin) 를 PATH 에 추가하고 소스를 직접 컴파일할 수도 있음
- 호스트: `pip install pyserial` (tkinter 는 기본 포함)

## 상태
- USB CDC 부트로더 컴파일/링크 확인 (-Os 약 17KB), 호스트는 포트 재열거 대응(sync 재시도)과 VID:PID 자동선택 추가
- 실제 보드 테스트는 아직 안 함 (USB 열거, 업로드, 앱 점프 후 재열거 모두 미검증)

## 규칙
- 펌웨어 프로토콜을 바꾸면 host/uart_flash.py 도 같이 수정
- 응답과 주석은 한국어
