#ifndef PROFILE_RECORD_H
#define PROFILE_RECORD_H
/**
 * @file   profile_record.h
 * @brief  The stored form of a profile, a set of amounts: a few bytes with a
 *         version and a checksum, so that storage which was never written,
 *         was cleared or was cut off part way is told apart from a profile.
 *         Pure logic with no hardware access; compiles unchanged for the MCU
 *         and for a PC.
 * @author BonelessPig
 *
 * The record is PROFILE_RECORD_SIZE bytes:
 *   byte 0         the version of this layout, 1
 *   bytes 1 to 14  the seven amounts, in the caller's order, each as two
 *                  bytes with the low byte first
 *   byte 15        a checksum of bytes 0 to 14: CRC-8, polynomial 0x07,
 *                  starting from 0
 * Erased storage (every byte 0xFF) and storage of all zeros both fail the
 * version check, so a record is cleared by writing PROFILE_RECORD_BLANK over
 * its first byte alone. A change to the layout takes a new version number,
 * and records of an old version are then refused, not misread.
 *
 * The record gives the amounts no meaning. The caller says what each is.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

#define PROFILE_AMOUNT_COUNT (7U)  // Amounts in a profile
#define PROFILE_RECORD_SIZE  (16U) // Bytes in a record

// Written over a record's first byte, this makes it no record
#define PROFILE_RECORD_BLANK (0xFFU)

/**
 * @brief  Writes a set of amounts out as a record. Reads p_amounts, writes
 *         p_bytes, and has no other effect.
 * @param  p_amounts  The amounts to store, PROFILE_AMOUNT_COUNT of them,
 *                    each 0 to 65535 and kept as given.
 * @param  p_bytes    Receives the record, PROFILE_RECORD_SIZE bytes.
 * @note   Does nothing if either pointer is null.
 */
void profile_record_encode(const uint16_t *p_amounts, uint8_t *p_bytes);

/**
 * @brief  Reads a set of amounts back from a record, if it is one. Reads
 *         p_bytes, writes p_amounts, and has no other effect.
 * @param  p_bytes    The stored bytes, PROFILE_RECORD_SIZE of them.
 * @param  p_amounts  Receives the amounts, PROFILE_AMOUNT_COUNT of them.
 *                    Left as they were unless the result is true.
 * @return true if the bytes are a record of this version with a good
 *         checksum; false otherwise, and if either pointer is null.
 */
bool profile_record_decode(const uint8_t *p_bytes, uint16_t *p_amounts);

#endif /* PROFILE_RECORD_H */
