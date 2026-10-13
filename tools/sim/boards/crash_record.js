'use strict';
/**
 * A crash log record as the firmware stores it, worked out here on its own
 * so that a scenario can check the bytes in the EEPROM against something
 * other than the firmware's own code: the record's number, the reset flags,
 * the place the firmware had got to, how many times running it happened,
 * and a CRC-8 (polynomial 0x07) of those four that starts from the layout's
 * version, 1. The log is eight such records from address 16. readCrashLog()
 * goes the other way, for the board to show what the log holds.
 */
const RECORD_VERSION = 1;
const CRC_POLYNOMIAL = 0x07;
const CRC_TOP_BIT = 0x80;
const BYTE_MASK = 0xFF;
const BITS_PER_BYTE = 8;

const CRASH_LOG_ADDRESS = 16;
const CRASH_RECORD_BYTES = 5;
const CRASH_RECORD_COUNT = 8;
const CRASH_LOG_BYTES = CRASH_RECORD_BYTES * CRASH_RECORD_COUNT;
const NO_CRASH_RECORD = -1; // In place of a slot: the log holds no record

function crc8(bytes, start) {
    let crc = start;

    for (const byte of bytes) {
        crc ^= byte;
        for (let bit = 0; bit < BITS_PER_BYTE; bit++) {
            crc = (crc & CRC_TOP_BIT)
                ? ((crc << 1) ^ CRC_POLYNOMIAL) & BYTE_MASK
                : (crc << 1) & BYTE_MASK;
        }
    }
    return crc;
}

/**
 * Lays a crash out as a record.
 * @param number  Which record it is, 0 to 255.
 * @param cause   The reset flags; 8 is the watchdog.
 * @param place   Where the firmware had got to.
 * @param count   How many times running it happened, 1 to 255.
 * @return The record's five bytes.
 */
function crashRecord(number, cause, place, count) {
    const bytes = [number, cause, place, count];

    bytes.push(crc8(bytes, RECORD_VERSION));
    return bytes;
}

/**
 * Reads a crash log, for something that shows it. A slot whose checksum is
 * wrong holds no record. The newest record is the first, from slot 0, that
 * is not followed by the record numbered one above it; the slot after the
 * last is the first, and the number after 255 is 0.
 * @param log  The log's forty bytes.
 * @return { newest, records }. records has an entry for each slot that
 *         holds a record, in slot order: { slot, number, cause, place,
 *         count }. newest is the slot of the newest, or NO_CRASH_RECORD.
 */
function readCrashLog(log) {
    const slots = [];

    for (let slot = 0; slot < CRASH_RECORD_COUNT; slot++) {
        const at = slot * CRASH_RECORD_BYTES;
        const [number, cause, place, count, checksum] = log.slice(at, at + CRASH_RECORD_BYTES);
        const isRecord = crc8([number, cause, place, count], RECORD_VERSION) === checksum;

        slots.push(isRecord ? { slot, number, cause, place, count } : null);
    }

    const records = slots.filter((record) => record !== null);
    const newest = records.find((record) => {
        const next = slots[(record.slot + 1) % CRASH_RECORD_COUNT];

        return (next === null) || (next.number !== ((record.number + 1) & BYTE_MASK));
    });

    return { newest: (newest === undefined) ? NO_CRASH_RECORD : newest.slot, records };
}

module.exports = {
    crashRecord, readCrashLog, NO_CRASH_RECORD,
    CRASH_LOG_ADDRESS, CRASH_RECORD_BYTES, CRASH_LOG_BYTES,
};
