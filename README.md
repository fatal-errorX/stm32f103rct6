# STM32F103RCT6 UART 부트로더

## 메모리 맵

| 영역 | 주소 | 크기 |
|---|---|---|
| 부트로더 | `0x08000000` ~ `0x08003FFF` | 16KB (실제 사용 약 2KB) |
| 애플리케이션 | `0x08004000` ~ `0x0803FFFF` | 240KB |

- 통신: USART1, **PA9 = TX, PA10 = RX**, 115200 8N1
- 클럭: HSI 8MHz (외부 크리스탈이 없어도 동작)

## 부팅 동작

1. 앱이 `BKP->DR1 = 0xB007` 을 쓰고 리셋했다면 → 부트로더에 머묾
2. 앱 영역이 비었거나 유효하지 않으면 → 부트로더에 머묾
3. 그 외에는 **500ms** 동안 PING 을 기다리고, 오지 않으면 앱으로 점프

다운로드 도중 전원이 끊겨도 벽돌이 되지 않도록, 앱의 첫 8바이트(초기 SP, Reset 벡터)는 CRC 검증이 통과한 뒤 **마지막에** 기록합니다. 중간에 끊긴 이미지는 무효로 판정되어 다음 부팅 때 부트로더에 머뭅니다.

## 1. 부트로더 빌드 및 굽기

```bash
cd bootloader
make
```

`build/bootloader.hex` 를 ST-LINK (STM32CubeProgrammer 등)로 한 번 굽습니다.

## 2. 애플리케이션 쪽 수정 (필수)

### 링커 스크립트 (CubeIDE: `STM32F103RCTX_FLASH.ld`)

```
FLASH (rx) : ORIGIN = 0x08004000, LENGTH = 240K
```

Keil 이면 *Options → Target → IROM1*: Start `0x8004000`, Size `0x3C000`.

### 벡터 테이블 오프셋 (`system_stm32f1xx.c`)

```c
#define USER_VECT_TAB_ADDRESS
#define VECT_TAB_OFFSET  0x00004000U
```

(부트로더가 점프 전에 VTOR 을 설정하지만, 앱 단독 디버깅 때도 맞게 동작하도록 설정해 두는 것을 권장합니다.)

### .bin 생성

```bash
arm-none-eabi-objcopy -O binary app.elf app.bin
```

CubeIDE: *Project Properties → C/C++ Build → Settings → MCU Post build outputs → Convert to binary file* 체크.

### (선택) 앱에서 부트로더로 진입

리셋 버튼 없이 업데이트하려면 앱에 다음 함수를 넣고, 원하는 시점(예: UART 로 특정 명령 수신)에 호출하세요.

```c
void enter_bootloader(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    BKP->DR1 = 0xB007;
    NVIC_SystemReset();
}
```

## 3. 펌웨어 다운로드

```bash
pip install pyserial
```

```bash
python host/uart_flash.py COM5 app.bin
```

`[1/5] 부트로더 접속 중...` 이 뜨면 보드의 리셋 버튼을 누르세요. 그 뒤 지우기 → 쓰기 → CRC 검증 → 앱 실행까지 자동으로 진행됩니다. 옵션: `--baud`, `--no-run`, `--wait 초`.

### GUI

```bash
python host/uart_flash_gui.py
```

포트와 `.bin` 파일을 고르고 **다운로드**를 누른 뒤 보드를 리셋하면 됩니다. 진행률, 로그가 표시되고 언제든 **취소**할 수 있습니다. 쓰는 도중 취소하면 앱이 무효 상태로 남아 다음 부팅 때 부트로더에 머물기 때문에 다시 받으면 됩니다.

exe 로 배포하려면:

```bash
pip install pyinstaller
pyinstaller --onefile --windowed --name STM32Flasher host/uart_flash_gui.py
```

## 프로토콜

```
요청: A5 | CMD | LEN(2,LE) | PAYLOAD | CRC16(2,LE)          CRC16 범위: CMD..PAYLOAD
응답: A5 | CMD | STATUS | LEN(2,LE) | DATA | CRC16(2,LE)    CRC16 범위: CMD..DATA
CRC16 = CCITT-FALSE (poly 0x1021, init 0xFFFF)
```

| CMD | 이름 | Payload | 응답 Data |
|---|---|---|---|
| 0x01 | PING | - | ver(2) app_start(4) app_max(4) max_chunk(2) |
| 0x02 | ERASE | size(4) | - |
| 0x03 | WRITE | offset(4) + data(짝수, 최대 1024) | - |
| 0x04 | VERIFY | size(4) + crc32(4) (zlib 방식) | - |
| 0x05 | RUN | - | (응답 후 앱으로 점프) |

STATUS: 0 OK, 1 BAD_CRC, 2 BAD_CMD, 3 BAD_ARG, 4 FLASH_ERR, 5 VERIFY_FAIL, 6 NO_APP

## 설정 변경 (`bootloader/config.h`)

- `BL_BAUDRATE`, `BOOT_WAIT_MS`, `MAX_CHUNK`, `BL_MAGIC`
- 부트로더 크기를 바꾸려면 `BL_SIZE` 와 `stm32f103rc_bl.ld` 의 FLASH LENGTH 를 같이 수정하고, 앱 링커 시작 주소도 맞춰야 합니다.
