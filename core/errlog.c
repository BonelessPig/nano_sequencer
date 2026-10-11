/**
 * @file   errlog.c
 * @brief  Crash log implementation. Pure logic: no hardware access.
 * @author BonelessPig
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "errlog.h"
#include <stddef.h>
#include "crc8.h"

#define RECORD_VERSION  (1U) // The layout described in errlog.h; starts the checksum
#define NUMBER_OFFSET   (0U)
#define CAUSE_OFFSET    (1U)
#define PLACE_OFFSET    (2U)
#define COUNT_OFFSET    (3U)
#define CHECKSUM_OFFSET (ERRLOG_RECORD_SIZE - 1U) // Everything before it is summed

#define CAUSE_WATCHDOG (0x08U) // The cause that makes a start-up a crash
#define FIRST_NUMBER   (1U)
#define COUNT_MAX      (255U)
#define NO_SLOT        (ERRLOG_RECORD_COUNT) // In place of a slot: there is none



/**
 * @brief  Gives where a record begins in the log.
 * @param  slot  Which record, 0 to ERRLOG_RECORD_COUNT - 1.
 * @return Its offset in bytes.
 */
static uint8_t slot_offset(uint8_t slot)
{
    return (uint8_t)(slot * ERRLOG_RECORD_SIZE);
}



/**
 * @brief  Gives the slot after a slot, going round from the last to the
 *         first.
 * @param  slot  0 to ERRLOG_RECORD_COUNT - 1.
 * @return The next slot.
 */
static uint8_t slot_after(uint8_t slot)
{
    return (uint8_t)((slot + 1U) % ERRLOG_RECORD_COUNT);
}



/**
 * @brief  Tells whether a slot of the log holds a record.
 * @param  p_log  The log; not null.
 * @param  slot   0 to ERRLOG_RECORD_COUNT - 1.
 * @return true if its checksum is right.
 */
static bool is_record(const uint8_t *p_log, uint8_t slot)
{
    const uint8_t *p_record = &p_log[slot_offset(slot)];

    return crc8(RECORD_VERSION, p_record, (uint8_t)CHECKSUM_OFFSET) ==
           p_record[CHECKSUM_OFFSET];
}



/**
 * @brief  Finds the newest record: the first that is not followed by the
 *         record numbered one above it.
 * @param  p_log  The log; not null.
 * @return Its slot, or NO_SLOT if the log holds no record.
 */
static uint8_t newest_slot(const uint8_t *p_log)
{
    uint8_t newest = NO_SLOT;

    for (uint8_t slot = 0U; (slot < ERRLOG_RECORD_COUNT) && (NO_SLOT == newest); slot++)
    {
        if (is_record(p_log, slot))
        {
            const uint8_t next   = slot_after(slot);
            const uint8_t number = p_log[slot_offset(slot) + NUMBER_OFFSET];
            const uint8_t after  = (uint8_t)(number + 1U); // Goes round to 0

            if (!is_record(p_log, next) ||
                (p_log[slot_offset(next) + NUMBER_OFFSET] != after))
            {
                newest = slot;
            }
        }
    }

    return newest;
}



/**
 * @brief  Writes a record into a slot of the log, with its checksum.
 * @param  p_log    The log; not null.
 * @param  slot     0 to ERRLOG_RECORD_COUNT - 1.
 * @param  p_entry  What the record is to hold; not null.
 */
static void write_record(uint8_t *p_log, uint8_t slot, const errlog_entry_t *p_entry)
{
    uint8_t *p_record = &p_log[slot_offset(slot)];

    p_record[NUMBER_OFFSET]   = p_entry->number;
    p_record[CAUSE_OFFSET]    = p_entry->cause;
    p_record[PLACE_OFFSET]    = p_entry->place;
    p_record[COUNT_OFFSET]    = p_entry->count;
    p_record[CHECKSUM_OFFSET] = crc8(RECORD_VERSION, p_record, (uint8_t)CHECKSUM_OFFSET);
}



/**
 * @brief  Reads the record in a slot of the log.
 * @param  p_log    The log; not null.
 * @param  slot     A slot that holds a record.
 * @param  p_entry  Receives it; not null.
 */
static void read_record(const uint8_t *p_log, uint8_t slot, errlog_entry_t *p_entry)
{
    const uint8_t *p_record = &p_log[slot_offset(slot)];

    p_entry->number = p_record[NUMBER_OFFSET];
    p_entry->cause  = p_record[CAUSE_OFFSET];
    p_entry->place  = p_record[PLACE_OFFSET];
    p_entry->count  = p_record[COUNT_OFFSET];
}



/**
 * @brief  Works out the record a crash is to be kept in: the newest one with
 *         its count raised if it is of the same crash, and otherwise a new
 *         one in the slot after it.
 * @param  p_log    The log; not null.
 * @param  p_entry  The crash, with its cause and place filled in; receives
 *                  the number and count of the record to write. Not null.
 * @return The slot to write it to, or NO_SLOT if the newest record is of the
 *         same crash and can count no higher.
 */
static uint8_t slot_for(const uint8_t *p_log, errlog_entry_t *p_entry)
{
    const uint8_t  newest = newest_slot(p_log);
    uint8_t        slot   = 0U;
    errlog_entry_t last;

    p_entry->number = FIRST_NUMBER;
    p_entry->count  = 1U;

    if (NO_SLOT != newest)
    {
        read_record(p_log, newest, &last);

        if ((last.cause != p_entry->cause) || (last.place != p_entry->place))
        {
            slot            = slot_after(newest);
            p_entry->number = (uint8_t)(last.number + 1U); // Goes round to 0
        }
        else if (last.count < COUNT_MAX)
        {
            slot            = newest;
            p_entry->number = last.number;
            p_entry->count  = (uint8_t)(last.count + 1U);
        }
        else
        {
            slot = NO_SLOT;
        }
    }

    return slot;
}



bool errlog_add(uint8_t *p_log, uint8_t cause, uint8_t place, uint8_t *p_offset)
{
    bool b_changed = false;

    if ((NULL != p_log) && (NULL != p_offset) && (0U != (cause & CAUSE_WATCHDOG)))
    {
        errlog_entry_t entry;
        uint8_t        slot;

        entry.cause = cause;
        entry.place = place;
        slot        = slot_for(p_log, &entry);

        if (NO_SLOT != slot)
        {
            write_record(p_log, slot, &entry);
            *p_offset = slot_offset(slot);
            b_changed = true;
        }
    }

    return b_changed;
}



bool errlog_newest(const uint8_t *p_log, errlog_entry_t *p_entry)
{
    bool b_found = false;

    if ((NULL != p_log) && (NULL != p_entry))
    {
        const uint8_t newest = newest_slot(p_log);

        if (NO_SLOT != newest)
        {
            read_record(p_log, newest, p_entry);
            b_found = true;
        }
    }

    return b_found;
}
