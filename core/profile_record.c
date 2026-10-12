/**
 * @file   profile_record.c
 * @brief  Profile record implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "profile_record.h"
#include <stddef.h>
#include "crc8.h"

#define RECORD_VERSION   (1U) // The layout described in profile_record.h
#define VERSION_OFFSET   (0U)
#define AMOUNTS_OFFSET   (1U)
#define BYTES_PER_AMOUNT (2U)
#define CHECKSUM_OFFSET  (PROFILE_RECORD_SIZE - 1U) // Everything before it is summed

#define BITS_PER_BYTE    (8U)
#define LOW_BYTE_MASK    (0xFFU)
#define CRC_START        (0U) // The standard checksum (see crc8.h)



void profile_record_encode(const uint16_t *p_amounts, uint8_t *p_bytes)
{
    if ((NULL != p_amounts) && (NULL != p_bytes))
    {
        p_bytes[VERSION_OFFSET] = RECORD_VERSION;

        for (uint8_t i = 0U; i < PROFILE_AMOUNT_COUNT; i++)
        {
            const uint8_t  at     = (uint8_t)(AMOUNTS_OFFSET + (i * BYTES_PER_AMOUNT));
            const uint16_t amount = p_amounts[i];

            p_bytes[at]      = (uint8_t)(amount & LOW_BYTE_MASK);
            p_bytes[at + 1U] = (uint8_t)(amount >> BITS_PER_BYTE);
        }

        p_bytes[CHECKSUM_OFFSET] = crc8(CRC_START, p_bytes, (uint8_t)CHECKSUM_OFFSET);
    }
}



bool profile_record_decode(const uint8_t *p_bytes, uint16_t *p_amounts)
{
    bool b_valid = false;

    if ((NULL != p_bytes) && (NULL != p_amounts))
    {
        b_valid = (RECORD_VERSION == p_bytes[VERSION_OFFSET]) &&
                  (crc8(CRC_START, p_bytes, (uint8_t)CHECKSUM_OFFSET) ==
                   p_bytes[CHECKSUM_OFFSET]);
    }

    if (b_valid)
    {
        for (uint8_t i = 0U; i < PROFILE_AMOUNT_COUNT; i++)
        {
            const uint8_t  at   = (uint8_t)(AMOUNTS_OFFSET + (i * BYTES_PER_AMOUNT));
            const uint16_t low  = p_bytes[at];
            const uint16_t high = p_bytes[at + 1U];

            p_amounts[i] = (uint16_t)((uint16_t)(high << BITS_PER_BYTE) | low);
        }
    }

    return b_valid;
}
