'use strict';
/**
 * The pitch calibration record as the firmware stores it, worked out here on
 * its own so that a scenario can check the bytes in the EEPROM against
 * something other than the firmware's own code: a version byte (1), five
 * 16-bit entries with the low byte first, and a CRC-8 (polynomial 0x07, from
 * 0) of what goes before it.
 */
const RECORD_VERSION = 1;
const CRC_POLYNOMIAL = 0x07;
const CRC_TOP_BIT = 0x80;
const BYTE_MASK = 0xFF;
const BITS_PER_BYTE = 8;

function crc8(bytes) {
    let crc = 0;

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
 * Lays a table out as a record.
 * @param table  The five entries, lowest octave first, each 0 to 65535.
 * @return The record's bytes.
 */
function calRecord(table) {
    const bytes = [RECORD_VERSION];

    for (const entry of table) {
        bytes.push(entry & BYTE_MASK, entry >> BITS_PER_BYTE);
    }
    bytes.push(crc8(bytes));
    return bytes;
}

module.exports = { calRecord };
