#ifndef ERRLOG_H
#define ERRLOG_H
/**
 * @file   errlog.h
 * @brief  The crash log: a short history of the times the watchdog has
 *         restarted the firmware, as the bytes that are kept in
 *         non-volatile storage. Pure logic with no hardware access; compiles
 *         unchanged for the MCU and for a PC.
 * @author BonelessPig
 *
 * The log is ERRLOG_SIZE bytes: ERRLOG_RECORD_COUNT records of
 * ERRLOG_RECORD_SIZE bytes each, used as a ring. A record is:
 *   byte 0  its number: 1 for the first crash recorded, one more for each
 *           record after it, going from 255 back to 0
 *   byte 1  what reset the processor, a bit for each cause (bit 3, value 8,
 *           being the watchdog)
 *   byte 2  the place the firmware had got to, as the caller numbers them
 *   byte 3  how many times running this has happened, 1 to 255
 *   byte 4  a checksum of bytes 0 to 3: CRC-8, polynomial 0x07, starting
 *           from the version of this layout, 1
 * Erased storage (every byte 0xFF) and storage of all zeros both fail the
 * checksum, and so does a record of another version.
 *
 * The newest record is the first, counting from the start of the log, that
 * is not followed by the record numbered one above it. A crash with the same
 * cause and place as the newest record raises that record's count and takes
 * no new one, so a firmware that crashes over and over the same way writes
 * to the storage at most 255 times and keeps the history before it.
 *
 * @copyright Copyright (c) 2026
 *
 */
#include <stdbool.h>
#include <stdint.h>

#define ERRLOG_RECORD_SIZE  (5U) // Bytes in a record
#define ERRLOG_RECORD_COUNT (8U) // Records in the log
#define ERRLOG_SIZE         (ERRLOG_RECORD_SIZE * ERRLOG_RECORD_COUNT) // Bytes in the log

/**
 * @brief One record of the log, read out.
 */
typedef struct
{
    uint8_t number; // Which record it is: 1 for the first, 0 after 255
    uint8_t cause;  // What reset the processor, a bit for each cause
    uint8_t place;  // Where the firmware had got to
    uint8_t count;  // Times running it happened, 1 to 255
} errlog_entry_t;

/**
 * @brief  Records a start-up in the log, if it was a crash: that is, if the
 *         watchdog is among its causes (bit 3 of cause). Reads and writes
 *         p_log, writes *p_offset, and has no other effect. At most one
 *         record changes.
 * @param  p_log     The log as stored, ERRLOG_SIZE bytes. Anything in it that
 *                   is not a record is taken as an empty place.
 * @param  cause     What reset the processor, a bit for each cause.
 * @param  place     Where the firmware had got to, any value.
 * @param  p_offset  Receives where in p_log the record that changed begins,
 *                   0 to ERRLOG_SIZE - ERRLOG_RECORD_SIZE; those
 *                   ERRLOG_RECORD_SIZE bytes are what to store. Left as it
 *                   was unless the result is true.
 * @return true if a record was added or its count raised; false if the
 *         start-up was not a crash, if the newest record is of the same
 *         crash and its count is already 255, and if either pointer is null.
 */
bool errlog_add(uint8_t *p_log, uint8_t cause, uint8_t place, uint8_t *p_offset);

/**
 * @brief  Gives the newest record of the log. Reads p_log, writes *p_entry,
 *         and has no other effect.
 * @param  p_log    The log as stored, ERRLOG_SIZE bytes.
 * @param  p_entry  Receives the record. Left as it was unless the result is
 *                  true.
 * @return true if the log holds a record; false if it holds none, and if
 *         either pointer is null.
 */
bool errlog_newest(const uint8_t *p_log, errlog_entry_t *p_entry);

#endif /* ERRLOG_H */
