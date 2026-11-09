#include "crc.h"

uint16_t crc16_update(uint16_t crc, const uint8_t *data, uint32_t len)
{
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
    }
    return crc;
}

/* 4bit 테이블: 코드 작고, 8MHz 에서도 240KB 를 1초 이내로 계산 */
static const uint32_t crc32_nibble[16] = {
    0x00000000UL, 0x1DB71064UL, 0x3B6E20C8UL, 0x26D930ACUL,
    0x76DC4190UL, 0x6B6B51F4UL, 0x4DB26158UL, 0x5005713CUL,
    0xEDB88320UL, 0xF00F9344UL, 0xD6D6A3E8UL, 0xCB61B38CUL,
    0x9B64C2B0UL, 0x86D3D2D4UL, 0xA00AE278UL, 0xBDBDF21CUL,
};

uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    crc = ~crc;
    while (len--) {
        crc ^= *data++;
        crc = (crc >> 4) ^ crc32_nibble[crc & 0xFU];
        crc = (crc >> 4) ^ crc32_nibble[crc & 0xFU];
    }
    return ~crc;
}
