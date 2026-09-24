#ifndef CRC8_H
#define CRC8_H

#include <stddef.h>
#include <stdint.h>

#define CRC8_POLY  0x07u
#define CRC8_INIT  0x00u

/* CRC-8/SMBUS: poly 0x07, init 0x00, no reflection, no final XOR. */
uint8_t crc8(const uint8_t *data, size_t len);

#endif /* CRC8_H */
