/*
 * bl_io 의 USB CDC(가상 COM 포트) 구현.
 *
 * CubeMX 가 만든 usbd_cdc_if.c / usb_device.c 는 수정하지 않는다.
 * bl_io_init() 에서 CDC 인터페이스 콜백 테이블(fops)만 이 파일의 것으로 교체한다.
 * (클래스 Init 은 호스트가 SET_CONFIGURATION 을 보낼 때 불리므로 그 전에 교체하면 된다)
 */
#include <string.h>
#include "main.h"
#include "usbd_core.h"
#include "usbd_cdc.h"
#include "bl_config.h"
#include "bl_io.h"
#include "bl_log.h"

extern USBD_HandleTypeDef hUsbDeviceFS;     /* usb_device.c */

/* ---------------- 수신: 링 버퍼 (ISR 이 쓰고, 메인이 읽음) ---------------- */

#define RX_RING_SIZE    2048U               /* 2의 거듭제곱, 패킷 최대 크기(~1034B)보다 커야 함 */
#define RX_RING_MASK    (RX_RING_SIZE - 1U)
#define USB_PKT         CDC_DATA_FS_MAX_PACKET_SIZE

static uint8_t           s_rx_pkt[USB_PKT];     /* USB 가 직접 채우는 버퍼 */
static uint8_t           s_ring[RX_RING_SIZE];
static volatile uint32_t s_head;                /* ISR 이 증가 */
static volatile uint32_t s_tail;                /* 메인이 증가 */
static volatile uint8_t  s_rx_stalled;          /* 링이 꽉 차서 다음 OUT 패킷 수신을 보류 중 */
static volatile uint32_t s_rx_total;            /* 디버그용: 지금까지 받은 바이트 수 */

static uint32_t ring_free(void)
{
    return RX_RING_SIZE - (s_head - s_tail);
}

/* ---------------- CDC 콜백 ---------------- */

static USBD_CDC_LineCodingTypeDef s_line = { 115200, 0, 0, 8 };

static int8_t cdc_init(void)
{
    USBD_CDC_SetRxBuffer(&hUsbDeviceFS, s_rx_pkt);
    USBD_CDC_SetTxBuffer(&hUsbDeviceFS, NULL, 0);
    s_head = s_tail = 0;
    s_rx_stalled = 0;
    return USBD_OK;
}

static int8_t cdc_deinit(void)
{
    return USBD_OK;
}

static int8_t cdc_control(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
    (void)length;
    switch (cmd) {
    case CDC_SET_LINE_CODING:       /* 보레이트 등은 의미 없지만 호스트가 읽어가므로 저장 */
        s_line.bitrate    = (uint32_t)(pbuf[0] | (pbuf[1] << 8) | (pbuf[2] << 16) | ((uint32_t)pbuf[3] << 24));
        s_line.format     = pbuf[4];
        s_line.paritytype = pbuf[5];
        s_line.datatype   = pbuf[6];
        break;
    case CDC_GET_LINE_CODING:
        pbuf[0] = (uint8_t)s_line.bitrate;
        pbuf[1] = (uint8_t)(s_line.bitrate >> 8);
        pbuf[2] = (uint8_t)(s_line.bitrate >> 16);
        pbuf[3] = (uint8_t)(s_line.bitrate >> 24);
        pbuf[4] = s_line.format;
        pbuf[5] = s_line.paritytype;
        pbuf[6] = s_line.datatype;
        break;
    default:                        /* SET_CONTROL_LINE_STATE(DTR/RTS) 등은 무시 */
        break;
    }
    return USBD_OK;
}

/* USB 인터럽트 컨텍스트에서 호출됨 */
static int8_t cdc_receive(uint8_t *buf, uint32_t *len)
{
    uint32_t n = *len;

    for (uint32_t i = 0; i < n; i++)
        s_ring[(s_head + i) & RX_RING_MASK] = buf[i];
    s_head += n;
    s_rx_total += n;

    if (ring_free() >= USB_PKT) {
        USBD_CDC_SetRxBuffer(&hUsbDeviceFS, s_rx_pkt);
        USBD_CDC_ReceivePacket(&hUsbDeviceFS);      /* 다음 패킷 수신 허용 */
    } else {
        s_rx_stalled = 1;                           /* 메인이 비우면 그때 재개 (그동안 호스트에는 NAK) */
    }
    return USBD_OK;
}

static USBD_CDC_ItfTypeDef s_fops = { cdc_init, cdc_deinit, cdc_control, cdc_receive };

/* ---------------- bl_io 구현 ---------------- */

static USBD_CDC_HandleTypeDef *cdc_handle(void)
{
    if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
        return NULL;
    return (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
}

void bl_io_log_status(void)
{
    /* dev_state: 0=초기 1=DEFAULT(리셋됨) 2=ADDRESSED 3=CONFIGURED(열거 완료) 4=SUSPENDED */
    BL_LOGV("[BL]   usb dev_state", hUsbDeviceFS.dev_state);
    /* SUSPENDED(4) 직전 상태: 0 이면 호스트의 버스 리셋을 한 번도 못 받은 것(호스트가 안 보임) */
    BL_LOGV("[BL]   usb old_state", hUsbDeviceFS.dev_old_state);
    BL_LOGV("[BL]   usb DADDR    ", USB->DADDR);      /* 하위 7비트가 0 이 아니면 호스트가 주소를 줬음(열거 진행) */
    BL_LOGV("[BL]   usb rx_total ", s_rx_total);
}

void bl_io_init(void)
{
    USBD_CDC_RegisterInterface(&hUsbDeviceFS, &s_fops);
}

int bl_io_getc(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (s_head == s_tail) {
        if ((HAL_GetTick() - start) >= timeout_ms)
            return -1;
    }

    uint8_t c = s_ring[s_tail & RX_RING_MASK];
    s_tail++;

    if (s_rx_stalled) {
        __disable_irq();            /* ISR 과의 경합 방지 */
        if (s_rx_stalled && ring_free() >= USB_PKT) {
            s_rx_stalled = 0;
            USBD_CDC_SetRxBuffer(&hUsbDeviceFS, s_rx_pkt);
            USBD_CDC_ReceivePacket(&hUsbDeviceFS);
        }
        __enable_irq();
    }
    return c;
}

/* 이전 전송이 끝날 때까지 대기. 연결이 없거나 시간 초과면 0 */
static int tx_wait_idle(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    USBD_CDC_HandleTypeDef *h;

    while ((h = cdc_handle()) != NULL) {
        if (h->TxState == 0)
            return 1;
        if ((HAL_GetTick() - start) >= timeout_ms)
            return 0;
    }
    return 0;
}

void bl_io_write(const uint8_t *buf, uint32_t len)
{
    static uint8_t tx[USB_PKT];

    while (len) {
        uint32_t n = (len > sizeof(tx)) ? sizeof(tx) : len;

        if (!tx_wait_idle(100))
            return;                 /* 호스트가 없거나 포트가 닫힘: 버린다 */
        memcpy(tx, buf, n);
        USBD_CDC_SetTxBuffer(&hUsbDeviceFS, tx, (uint16_t)n);
        if (USBD_CDC_TransmitPacket(&hUsbDeviceFS) != USBD_OK)
            return;
        buf += n;
        len -= n;
    }
}

void bl_io_flush(void)
{
    (void)tx_wait_idle(100);
}

/*
 * 이 보드는 D+ 에 1.5k 풀업이 고정되어 있어 소프트웨어로 분리할 수 없다.
 * 그래서 D+(PA12) 를 잠깐 LOW 로 눌러 호스트가 "뽑혔다 꽂힘"으로 인식하고
 * USB 를 새로 열거하게 한다. (안 하면 MCU 만 리셋됐을 때 호스트는 장치가 계속 붙어 있다고
 * 보고 재열거를 안 해서 포트가 안 나타나거나 suspend 에 머문다)
 */
static void usb_force_detach(void)
{
    GPIO_InitTypeDef g = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    g.Pin   = GPIO_PIN_12;
    g.Mode  = GPIO_MODE_OUTPUT_PP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
    HAL_Delay(USB_DETACH_MS);
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_12);        /* 입력(플로팅)으로 복귀 -> 풀업으로 올라감 */
}

void bl_io_prepare(void)
{
    BL_LOG("[BL] usb: force detach (D+ low)");
    usb_force_detach();
}

void bl_io_deinit(void)
{
    USBD_Stop(&hUsbDeviceFS);
    USBD_DeInit(&hUsbDeviceFS);     /* HAL_PCD_MspDeInit 에서 USB 클럭/IRQ 도 정리 */
    usb_force_detach();
}
