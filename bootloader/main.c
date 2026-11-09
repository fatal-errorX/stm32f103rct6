/*
 * STM32F103RCT6 UART bootloader
 *
 * 패킷 (호스트 -> 부트로더)
 *   0xA5 | CMD | LEN(2, LE) | PAYLOAD(LEN) | CRC16(2, LE)       CRC 범위: CMD..PAYLOAD
 * 응답 (부트로더 -> 호스트)
 *   0xA5 | CMD | STATUS | LEN(2, LE) | DATA(LEN) | CRC16(2, LE)  CRC 범위: CMD..DATA
 *
 * 전원 차단 대비: 앱 벡터 테이블의 첫 8바이트(SP, Reset 벡터)는 RAM 에 보관했다가
 * VERIFY 에서 CRC 가 맞을 때 마지막으로 기록한다. 중간에 끊기면 앱이 "무효"로 판정되어
 * 다음 부팅 때도 부트로더에 머문다.
 */
#include <stdint.h>
#include "config.h"
#include "regs.h"
#include "hal.h"
#include "crc.h"

#define SOF             0xA5U

#define CMD_PING        0x01U   /* -> ver(2) app_start(4) app_max(4) max_chunk(2) */
#define CMD_ERASE       0x02U   /* size(4) */
#define CMD_WRITE       0x03U   /* offset(4) data(N), offset/N 짝수 */
#define CMD_VERIFY      0x04U   /* size(4) crc32(4) */
#define CMD_RUN         0x05U

#define ST_OK           0x00U
#define ST_BAD_CRC      0x01U
#define ST_BAD_CMD      0x02U
#define ST_BAD_ARG      0x03U
#define ST_FLASH_ERR    0x04U
#define ST_VERIFY_FAIL  0x05U
#define ST_NO_APP       0x06U

#define HEAD_SIZE       8U      /* 마지막에 기록할 벡터 테이블 앞부분 */

static uint8_t  s_buf[4 + MAX_CHUNK];
static uint8_t  s_head[HEAD_SIZE];   /* 앱 첫 8바이트의 "의도된" 값 */
static uint8_t  s_head_dirty;        /* s_head 가 아직 플래시에 안 써졌음 */

/* ------------------------------------------------------------------ */

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static void load_head_from_flash(void)
{
    for (uint32_t i = 0; i < HEAD_SIZE; i++)
        s_head[i] = *(volatile uint8_t *)(APP_START_ADDR + i);
    s_head_dirty = 0;
}

static int app_is_valid(void)
{
    uint32_t sp = REG32(APP_START_ADDR);
    uint32_t pc = REG32(APP_START_ADDR + 4);

    if ((sp & 3U) || sp <= RAM_BASE_ADDR || sp > RAM_BASE_ADDR + RAM_SIZE)
        return 0;
    if (!(pc & 1U) || pc < APP_START_ADDR || pc >= APP_START_ADDR + APP_MAX_SIZE)
        return 0;
    return 1;
}

static void jump_to_app(void)
{
    uint32_t sp = REG32(APP_START_ADDR);
    uint32_t pc = REG32(APP_START_ADDR + 4);

    uart_flush();
    uart_deinit();
    systick_deinit();

    SCB_VTOR = APP_START_ADDR;
    __asm volatile ("dsb\n isb\n msr msp, %0\n bx %1\n" :: "r"(sp), "r"(pc) : "memory");
    for (;;)
        ;
}

/* ------------------------------------------------------------------ */

static void send_resp(uint8_t cmd, uint8_t status, const uint8_t *data, uint16_t len)
{
    uint8_t hdr[4] = { cmd, status, (uint8_t)len, (uint8_t)(len >> 8) };
    uint16_t crc = crc16_update(0xFFFFU, hdr, 4);
    crc = crc16_update(crc, data, len);

    uart_putc(SOF);
    uart_write(hdr, 4);
    uart_write(data, len);
    uart_putc((uint8_t)crc);
    uart_putc((uint8_t)(crc >> 8));
}

/*
 * 반환: 1 = 정상 패킷, 0 = 첫 바이트 타임아웃, -1 = 깨진 패킷(CRC/길이 오류, 바이트 간 타임아웃)
 */
static int recv_packet(uint8_t *cmd, uint16_t *len, uint32_t wait_ms)
{
    int c;
    uint32_t start = millis();

    do {
        uint32_t elapsed = millis() - start;
        if (elapsed >= wait_ms)
            return 0;
        c = uart_getc(wait_ms - elapsed);
        if (c < 0)
            return 0;
    } while (c != SOF);

    uint8_t hdr[3];
    for (int i = 0; i < 3; i++) {
        if ((c = uart_getc(BYTE_TIMEOUT_MS)) < 0)
            return -1;
        hdr[i] = (uint8_t)c;
    }
    *cmd = hdr[0];
    *len = (uint16_t)(hdr[1] | (hdr[2] << 8));
    if (*len > sizeof(s_buf))
        return -1;

    for (uint16_t i = 0; i < *len; i++) {
        if ((c = uart_getc(BYTE_TIMEOUT_MS)) < 0)
            return -1;
        s_buf[i] = (uint8_t)c;
    }

    uint8_t crc_lo, crc_hi;
    if ((c = uart_getc(BYTE_TIMEOUT_MS)) < 0) return -1;
    crc_lo = (uint8_t)c;
    if ((c = uart_getc(BYTE_TIMEOUT_MS)) < 0) return -1;
    crc_hi = (uint8_t)c;

    uint16_t crc = crc16_update(0xFFFFU, hdr, 3);
    crc = crc16_update(crc, s_buf, *len);
    return (crc == (uint16_t)(crc_lo | (crc_hi << 8))) ? 1 : -1;
}

/* ------------------------------------------------------------------ */

static uint8_t do_erase(uint32_t size)
{
    if (size == 0 || size > APP_MAX_SIZE)
        return ST_BAD_ARG;

    uint32_t pages = (size + FLASH_PAGE_SIZE - 1U) / FLASH_PAGE_SIZE;
    uint8_t st = ST_OK;

    flash_unlock();
    for (uint32_t i = 0; i < pages; i++) {
        if (flash_erase_page(APP_START_ADDR + i * FLASH_PAGE_SIZE)) {
            st = ST_FLASH_ERR;
            break;
        }
    }
    flash_lock();

    load_head_from_flash();   /* 지워졌으면 전부 0xFF */
    return st;
}

static uint8_t do_write(uint32_t off, const uint8_t *data, uint32_t n)
{
    if (n == 0 || (off & 1U) || (n & 1U) || off > APP_MAX_SIZE || n > APP_MAX_SIZE - off)
        return ST_BAD_ARG;

    uint8_t st = ST_OK;
    flash_unlock();
    for (uint32_t i = 0; i < n; i += 2) {
        uint32_t o = off + i;
        if (o < HEAD_SIZE) {
            s_head[o]     = data[i];
            s_head[o + 1] = data[i + 1];
            s_head_dirty  = 1;
            continue;
        }
        uint16_t hw = (uint16_t)(data[i] | (data[i + 1] << 8));
        if (flash_write_half(APP_START_ADDR + o, hw)) {
            st = ST_FLASH_ERR;
            break;
        }
    }
    flash_lock();
    return st;
}

static uint8_t do_verify(uint32_t size, uint32_t expect)
{
    if (size < HEAD_SIZE || size > APP_MAX_SIZE)
        return ST_BAD_ARG;

    uint32_t crc = crc32_update(0, s_head, HEAD_SIZE);
    crc = crc32_update(crc, (const uint8_t *)(APP_START_ADDR + HEAD_SIZE), size - HEAD_SIZE);
    if (crc != expect)
        return ST_VERIFY_FAIL;

    if (s_head_dirty) {
        uint8_t st = ST_OK;
        flash_unlock();
        for (uint32_t i = 0; i < HEAD_SIZE; i += 2) {
            uint16_t hw = (uint16_t)(s_head[i] | (s_head[i + 1] << 8));
            if (flash_write_half(APP_START_ADDR + i, hw)) {
                st = ST_FLASH_ERR;
                break;
            }
        }
        flash_lock();
        load_head_from_flash();
        if (st != ST_OK)
            return st;
    }
    return app_is_valid() ? ST_OK : ST_NO_APP;
}

static void handle_packet(uint8_t cmd, uint16_t len)
{
    uint8_t st;

    switch (cmd) {
    case CMD_PING: {
        uint8_t info[12];
        info[0] = BL_VERSION_MAJOR;
        info[1] = BL_VERSION_MINOR;
        put_u32(&info[2], APP_START_ADDR);
        put_u32(&info[6], APP_MAX_SIZE);
        info[10] = (uint8_t)MAX_CHUNK;
        info[11] = (uint8_t)(MAX_CHUNK >> 8);
        send_resp(cmd, ST_OK, info, sizeof(info));
        return;
    }
    case CMD_ERASE:
        st = (len == 4) ? do_erase(get_u32(s_buf)) : ST_BAD_ARG;
        break;
    case CMD_WRITE:
        st = (len > 4) ? do_write(get_u32(s_buf), &s_buf[4], len - 4U) : ST_BAD_ARG;
        break;
    case CMD_VERIFY:
        st = (len == 8) ? do_verify(get_u32(s_buf), get_u32(&s_buf[4])) : ST_BAD_ARG;
        break;
    case CMD_RUN:
        if (!app_is_valid()) {
            st = ST_NO_APP;
            break;
        }
        send_resp(cmd, ST_OK, 0, 0);
        jump_to_app();
        return;
    default:
        st = ST_BAD_CMD;
        break;
    }
    send_resp(cmd, st, 0, 0);
}

int main(void)
{
    /* 앱이 BKP 매직을 남기고 리셋한 경우 무조건 부트로더에 머문다 */
    int stay = bkp_take_magic(BL_MAGIC);

    systick_init();
    uart_init(BL_BAUDRATE);
    load_head_from_flash();

    if (!app_is_valid())
        stay = 1;

    /* 대기 시간 안에 정상 패킷이 하나라도 오면 업데이트 모드로 진입 */
    uint32_t start = millis();
    while (!stay) {
        uint32_t elapsed = millis() - start;
        if (elapsed >= BOOT_WAIT_MS)
            jump_to_app();

        uint8_t cmd;
        uint16_t len;
        if (recv_packet(&cmd, &len, BOOT_WAIT_MS - elapsed) == 1) {
            stay = 1;
            handle_packet(cmd, len);
        }
    }

    for (;;) {
        uint8_t cmd;
        uint16_t len;
        int r = recv_packet(&cmd, &len, 1000);
        if (r == 1)
            handle_packet(cmd, len);
        else if (r < 0)
            send_resp(0, ST_BAD_CRC, 0, 0);
    }
}
