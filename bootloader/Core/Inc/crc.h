#ifndef CRC_H
#define CRC_H

#include <stdint.h>

/* CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) - 패킷 무결성 */
uint16_t crc16_update(uint16_t crc, const uint8_t *data, uint32_t len);

/* CRC-32 (zlib/Python zlib.crc32 와 동일). 처음엔 crc=0 으로 시작, 이어서 호출 가능 */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len);

#endif
