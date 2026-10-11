/**
 * @file   cal_record.c
 * @brief  Calibration record implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "cal_record.h"
#include <stddef.h>
#include "crc8.h"

#define RECORD_VERSION  (1U) // The layout described in cal_record.h
#define VERSION_OFFSET  (0U)
#define TABLE_OFFSET    (1U)
#define BYTES_PER_ENTRY (2U)
#define CHECKSUM_OFFSET (CAL_RECORD_SIZE - 1U) // Everything before it is summed

#define BITS_PER_BYTE   (8U)
#define LOW_BYTE_MASK   (0xFFU)
#define CRC_START       (0U) // The standard checksum (see crc8.h)



void cal_record_encode(const pitch_cal_config_t *p_table, uint8_t *p_bytes)
{
    if ((NULL != p_table) && (NULL != p_bytes))
    {
        p_bytes[VERSION_OFFSET] = RECORD_VERSION;

        for (uint8_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
        {
            const uint8_t  at = (uint8_t)(TABLE_OFFSET + (point * BYTES_PER_ENTRY));
            const uint16_t cv = p_table->cv[point];

            p_bytes[at]      = (uint8_t)(cv & LOW_BYTE_MASK);
            p_bytes[at + 1U] = (uint8_t)(cv >> BITS_PER_BYTE);
        }

        p_bytes[CHECKSUM_OFFSET] = crc8(CRC_START, p_bytes, (uint8_t)CHECKSUM_OFFSET);
    }
}



bool cal_record_decode(const uint8_t *p_bytes, pitch_cal_config_t *p_table)
{
    bool b_valid = false;

    if ((NULL != p_bytes) && (NULL != p_table))
    {
        b_valid = (RECORD_VERSION == p_bytes[VERSION_OFFSET]) &&
                  (crc8(CRC_START, p_bytes, (uint8_t)CHECKSUM_OFFSET) ==
                   p_bytes[CHECKSUM_OFFSET]);
    }

    if (b_valid)
    {
        for (uint8_t point = 0U; point < PITCH_CAL_POINT_COUNT; point++)
        {
            const uint8_t  at   = (uint8_t)(TABLE_OFFSET + (point * BYTES_PER_ENTRY));
            const uint16_t low  = p_bytes[at];
            const uint16_t high = p_bytes[at + 1U];

            p_table->cv[point] = (uint16_t)((uint16_t)(high << BITS_PER_BYTE) | low);
        }
    }

    return b_valid;
}
