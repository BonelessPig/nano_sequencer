'use strict';
/**
 * A profile of the amounts as the firmware stores it, and where: eight
 * records one after another from address 64 of the EEPROM, numbered as the
 * selector's positions are. A record is laid out as the calibration record
 * is, so cal_record.js works it out, away from the firmware's own code: a
 * version byte (1), seven 16-bit amounts with the low byte first, for
 * selector positions 1 to 7, and a CRC-8 of what goes before it. A cleared
 * profile has 0xFF for its first byte.
 */
const { calRecord } = require('./cal_record');

const PROFILE_ADDRESS = 64;
const PROFILE_RECORD_BYTES = 16;
const PROFILE_COUNT = 8;
const PROFILE_AMOUNTS = 7;
const PROFILES_END = PROFILE_ADDRESS + (PROFILE_COUNT * PROFILE_RECORD_BYTES);

/**
 * Lays seven amounts out as a record.
 * @param amounts  The amounts for selector positions 1 to 7, each 0 to 1023.
 * @return The record's bytes.
 */
function profileRecord(amounts) {
    if (amounts.length !== PROFILE_AMOUNTS) {
        throw new Error(`a profile is ${PROFILE_AMOUNTS} amounts, not ${amounts.length}`);
    }
    return calRecord(amounts);
}

/** Where a profile's record begins in the EEPROM; profile is 0 to 7. */
function profileAddress(profile) {
    return PROFILE_ADDRESS + (profile * PROFILE_RECORD_BYTES);
}

/**
 * An EEPROM image, from address 0 to the end of the profiles, erased but for
 * the records given.
 * @param records  An object of profile number to its seven amounts.
 * @return The bytes, for createBoard()'s eeprom option.
 */
function eepromWithProfiles(records) {
    const bytes = new Array(PROFILES_END).fill(0xFF);

    for (const [profile, amounts] of Object.entries(records)) {
        bytes.splice(profileAddress(Number(profile)), PROFILE_RECORD_BYTES,
            ...profileRecord(amounts));
    }
    return bytes;
}

module.exports = {
    profileRecord, profileAddress, eepromWithProfiles,
    PROFILE_ADDRESS, PROFILE_RECORD_BYTES, PROFILES_END,
};
