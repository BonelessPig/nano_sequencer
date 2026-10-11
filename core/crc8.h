#ifndef CRC8_H
#define CRC8_H
/**
 * @file   crc8.h
 * @brief  The checksum the stored records carry: CRC-8, polynomial 0x07 (x^8 +
 *         x^2 + x + 1), most significant bit first, with nothing reflected
 *         and nothing inverted at the end. Pure logic with no hardware access;
 *         compiles unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdint.h>

/**
 * @brief  Works out the checksum of a run of bytes, one bit at a time, so
 *         that it needs no table. Reads the bytes and has no other effect.
 * @param  start    The value to begin from: 0 for the standard checksum
 *                  (which gives 0xF4 for the nine characters "123456789"),
 *                  or the result for the bytes before these, to carry on.
 * @param  p_bytes  The bytes to sum.
 * @param  count    How many of them, 0 to 255.
 * @return The checksum; start itself if p_bytes is null or count is 0.
 */
uint8_t crc8(uint8_t start, const uint8_t *p_bytes, uint8_t count);

#endif /* CRC8_H */
