/**
 * @file   crc8.c
 * @brief  CRC-8 implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "crc8.h"
#include <stdbool.h>
#include <stddef.h>

#define BITS_PER_BYTE  (8U)
#define CRC_POLYNOMIAL (0x07U) // x^8 + x^2 + x + 1, without its top bit
#define CRC_TOP_BIT    (0x80U)



/**
 * @brief  Takes one more byte into a checksum.
 * @param  crc    The checksum so far.
 * @param  value  The byte to add.
 * @return The checksum with the byte in it.
 */
static uint8_t add_byte(uint8_t crc, uint8_t value)
{
    uint8_t result = (uint8_t)(crc ^ value);

    for (uint8_t bit = 0U; bit < BITS_PER_BYTE; bit++)
    {
        const bool b_top = (0U != (result & CRC_TOP_BIT));

        result = (uint8_t)(result << 1U);
        if (b_top)
        {
            result = (uint8_t)(result ^ CRC_POLYNOMIAL);
        }
    }

    return result;
}



uint8_t crc8(uint8_t start, const uint8_t *p_bytes, uint8_t count)
{
    uint8_t crc = start;

    if (NULL != p_bytes)
    {
        for (uint8_t i = 0U; i < count; i++)
        {
            crc = add_byte(crc, p_bytes[i]);
        }
    }

    return crc;
}
