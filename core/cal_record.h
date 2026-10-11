#ifndef CAL_RECORD_H
#define CAL_RECORD_H
/**
 * @file   cal_record.h
 * @brief  The stored form of the pitch calibration table: a few bytes with a
 *         version and a checksum, so that storage which was never written,
 *         was written by other firmware or was cut off part way is told
 *         apart from a table. Pure logic with no hardware access; compiles
 *         unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * The record is CAL_RECORD_SIZE bytes:
 *   byte 0         the version of this layout, 1
 *   bytes 1 to 10  the five table entries, lowest octave first, each as two
 *                  bytes with the low byte first
 *   byte 11        a checksum of bytes 0 to 10: CRC-8, polynomial 0x07,
 *                  starting from 0
 * Erased storage (every byte 0xFF) and storage of all zeros both fail the
 * version check. A change to the layout takes a new version number, and
 * records of an old version are then refused, not misread.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>
#include "pitch_cal.h"

#define CAL_RECORD_SIZE (12U) // Bytes in a record

/**
 * @brief  Writes a table out as a record. Reads *p_table, writes p_bytes,
 *         and has no other effect.
 * @param  p_table  The calibration table to store.
 * @param  p_bytes  Receives the record, CAL_RECORD_SIZE bytes.
 * @note   Does nothing if either pointer is null.
 */
void cal_record_encode(const pitch_cal_config_t *p_table, uint8_t *p_bytes);

/**
 * @brief  Reads a table back from a record, if it is one. Reads p_bytes,
 *         writes *p_table, and has no other effect.
 * @param  p_bytes  The stored bytes, CAL_RECORD_SIZE of them.
 * @param  p_table  Receives the table. Left as it was unless the result is
 *                  true, so the caller can put its default there first.
 * @return true if the bytes are a record of this version with a good
 *         checksum; false otherwise, and if either pointer is null.
 */
bool cal_record_decode(const uint8_t *p_bytes, pitch_cal_config_t *p_table);

#endif /* CAL_RECORD_H */
