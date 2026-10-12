'use strict';
// The profiles of the amounts: the button that saves, recalls and clears
// them, the records in the EEPROM, and the one loaded at power-on, on the
// emulated board, for all three firmware images. An amount is seen through
// what the LFO does with it: the pitch the DAC holds as a gate rises, or
// how long the gate is.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { calRecord } = require('../boards/cal_record');
const { createBoard } = require('../boards/nano_sequencer');
const {
    profileRecord, profileAddress, eepromWithProfiles,
    PROFILE_ADDRESS, PROFILE_RECORD_BYTES, PROFILES_END,
} = require('../boards/profile_record');

const CONFIGS = ['debug', 'release', 'midi'];
const NOTE = 5;                  // On every step
const PANEL = new Array(16).fill(NOTE);
const READING_150_BPM = 480;     // Exactly 100 ms a step
const STEP_MS = 100;
const PULSE_MS = STEP_MS / 24;   // A clock pulse: gate lengths are counted in them
const TOP = 1023;                // Of the pot and of the LFO
const LFO_HALF_UP = 768;
const POSITION_NOTE = 1;         // Selector positions, which number the profiles too
const POSITION_GATE = 2;
const POSITION_CHANCE = 3;
const SCAN_MS = 20;              // More than two input scans
const SETTLE_MS = 60;            // The button's debounce, two scans and the tick after
const CLICK_MS = 150;
const SAVE_MS = 1200;            // Holds of over one second, five and ten
const CLEAR_MS = 5200;
const CLEAR_ALL_MS = 10200;
const WRITE_MS = 80;             // More than the 16 bytes of a record take
const ERASED = 0xFF;
const MILLIVOLTS_PER_OCTAVE = 1000;
const SEMITONES_PER_OCTAVE = 12;
const ACTION_RECALL = 1;         // As the log gives them
const ACTION_SAVE = 2;
const ACTION_CLEAR = 3;
const ACTION_CLEAR_ALL = 4;
// The note amount alone, and the gate amount alone, as a profile's seven
const NOTE_AMOUNTS = [TOP, 0, 0, 0, 0, 0, 0];
const GATE_AMOUNTS = [0, TOP, 0, 0, 0, 0, 0];

function noteMillivolts(noteValue) {
    return Math.round(((noteValue - 1) * MILLIVOLTS_PER_OCTAVE) / SEMITONES_PER_OCTAVE);
}

// A board powered up with the selector and the LFO already where they are
// given, playing the same note on every step at 150 BPM
function playingBoard(config, { eeprom, selector = 0, lfo } = {}) {
    const board = createBoard(config, { eeprom });

    board.setNotes(PANEL);
    board.setTempoReading(READING_150_BPM);
    board.setSelector(selector);
    if (lfo !== undefined) {
        board.setLfoReading(lfo);
    }
    board.runForMs(SCAN_MS);
    return board;
}

// Gives one position its amount: the pot down to meet the 0 it holds, and up
function setAmount(board, position, reading) {
    board.setSelector(position);
    board.setTempoReading(0);
    board.runForMs(SCAN_MS);
    board.setTempoReading(reading);
    board.runForMs(SCAN_MS);
}

// Holds the profile button down for a time and lets it go, and runs on
// until what it asked for has been started
function pressButton(board, holdMs) {
    board.setProfileButton(true);
    board.runForMs(holdMs);
    board.setProfileButton(false);
    board.runForMs(SETTLE_MS);
}

// The pitches the gates rose on, and the gates that finished, over a number
// of steps from now
function play(board, steps) {
    const from = board.machine.cpu.cycles;
    const fromMs = board.machine.cyclesToMs(from);

    board.runForMs(steps * STEP_MS);
    return {
        pitches: board.gate.rises().filter((cycle) => cycle > from)
            .map((cycle) => board.cvWriteBefore(cycle).millivolts),
        gates: board.gatePulsesMs().filter((pulse) => pulse.riseMs > fromMs),
    };
}

function assertAllEqual(values, expected, what) {
    assert.ok(values.length > 0, `${what}: nothing to check`);
    assert.deepEqual(values, new Array(values.length).fill(expected), what);
}

function assertGatesOf(gates, pulses, what) {
    assert.ok(gates.length > 0, `${what}: nothing to check`);
    for (const gate of gates) {
        assert.ok(Math.abs(gate.widthMs - (pulses * PULSE_MS)) < 1.1,
            `${what}: gate of ${gate.widthMs} ms`);
    }
}

function profileBytes(board, profile) {
    return board.eepromBytes(PROFILES_END)
        .slice(profileAddress(profile), profileAddress(profile) + PROFILE_RECORD_BYTES);
}

function profileNotices(board) {
    return board.serial.notices.map((notice) => notice.text)
        .filter((text) => text.startsWith('Profile '));
}

for (const config of CONFIGS) {
    test(`${config}: a hold of a second saves the amounts to the selected profile`, () => {
        const board = playingBoard(config);

        setAmount(board, POSITION_GATE, TOP);
        setAmount(board, POSITION_NOTE, TOP);

        // The selector is on position 1, so it is profile 1 that is saved,
        // and the steps go on at their 100 ms through the 55 ms it takes
        const from = board.machine.cpu.cycles;

        pressButton(board, SAVE_MS);
        board.runForMs(WRITE_MS);
        assert.deepEqual(profileBytes(board, 1), profileRecord([TOP, TOP, 0, 0, 0, 0, 0]));

        const rises = board.clockOut.rises().filter((cycle) => cycle > from)
            .map((cycle) => board.machine.cyclesToMs(cycle));

        assert.ok(rises.length >= 12, `${rises.length} steps`);
        for (let i = 1; i < rises.length; i++) {
            assert.ok(Math.abs((rises[i] - rises[i - 1]) - STEP_MS) < 0.1,
                `step of ${rises[i] - rises[i - 1]} ms`);
        }

        // Nothing else in the EEPROM was written
        const eeprom = board.eepromBytes(PROFILES_END);

        eeprom.splice(profileAddress(1), PROFILE_RECORD_BYTES);
        assertAllEqual(eeprom, ERASED, 'the rest of the EEPROM');
        if (config === 'debug') {
            assert.deepEqual(profileNotices(board), [`Profile 1 = ${ACTION_SAVE}`]);
        }
    });

    test(`${config}: the selected profile is loaded at power-on`, () => {
        const eeprom = eepromWithProfiles({ [POSITION_NOTE]: NOTE_AMOUNTS });
        const board = playingBoard(config, { eeprom, selector: POSITION_NOTE, lfo: TOP });

        // The LFO at its top with all of the note amount: seven places up
        // from the second step, the first having begun before the LFO was
        // read, and at the tempo the pot is at
        const played = play(board, 4);

        assert.equal(board.gate.rises().length, played.pitches.length + 1);
        assertAllEqual(played.pitches, noteMillivolts(NOTE + 7), 'pitches');
        assert.equal(played.pitches.length, 4);
        assert.deepEqual(board.eepromBytes(PROFILES_END), eeprom); // Read, not rewritten
        if (config === 'debug') {
            assert.deepEqual(profileNotices(board), [`Profile 1 = ${ACTION_RECALL}`]);
        }
    });

    test(`${config}: a profile that is not the selected one is not loaded at power-on`, () => {
        const eeprom = eepromWithProfiles({ [POSITION_NOTE]: NOTE_AMOUNTS });
        const board = playingBoard(config, { eeprom, selector: POSITION_GATE, lfo: TOP });

        assertAllEqual(play(board, 3).pitches, noteMillivolts(NOTE), 'pitches');
        assert.deepEqual(profileNotices(board), []);
    });

    test(`${config}: a click recalls the selected profile, and the pot waits to meet it`, () => {
        const eeprom = eepromWithProfiles({ [POSITION_GATE]: GATE_AMOUNTS });
        const board = playingBoard(config, { eeprom, lfo: LFO_HALF_UP });

        // Nothing loaded with the selector on the tempo: gates of half a step
        assertGatesOf(play(board, 3).gates, 12, 'before');

        // Profile 2 has all of the gate amount: 18 pulses with the LFO half
        // way up. The pot, at the tempo's reading, does not pull it down
        board.setSelector(POSITION_GATE);
        board.runForMs(SCAN_MS);
        pressButton(board, CLICK_MS);
        board.runForMs(STEP_MS);
        assertGatesOf(play(board, 3).gates, 18, 'recalled');

        // Until it has been turned up to meet it, and then down
        board.setTempoReading(TOP);
        board.runForMs(SCAN_MS);
        board.setTempoReading(0);
        board.runForMs(SCAN_MS + STEP_MS);
        assertGatesOf(play(board, 3).gates, 12, 'the pot again');
        assert.deepEqual(board.eepromBytes(PROFILES_END), eeprom);
        if (config === 'debug') {
            assert.deepEqual(profileNotices(board), [`Profile 2 = ${ACTION_RECALL}`]);
        }
    });

    test(`${config}: a click on a profile that holds nothing changes nothing`, () => {
        const board = playingBoard(config, { lfo: TOP });

        setAmount(board, POSITION_NOTE, TOP);
        pressButton(board, CLICK_MS);
        board.runForMs(STEP_MS);
        assertAllEqual(play(board, 3).pitches, noteMillivolts(NOTE + 7), 'pitches');
        assertAllEqual(board.eepromBytes(PROFILES_END), ERASED, 'the EEPROM');
        assert.deepEqual(profileNotices(board), []);
    });

    test(`${config}: a hold of five seconds clears the selected profile and no other`, () => {
        const eeprom = eepromWithProfiles(
            { [POSITION_NOTE]: NOTE_AMOUNTS, [POSITION_GATE]: GATE_AMOUNTS });
        const board = playingBoard(config, { eeprom, selector: POSITION_NOTE, lfo: TOP });

        pressButton(board, CLEAR_MS);
        board.runForMs(WRITE_MS);

        // One byte makes it no profile
        const expected = eeprom.slice();

        expected[profileAddress(POSITION_NOTE)] = ERASED;
        assert.deepEqual(board.eepromBytes(PROFILES_END), expected);

        // The amounts in use are as they were loaded
        assertAllEqual(play(board, 3).pitches, noteMillivolts(NOTE + 7), 'pitches');
        if (config === 'debug') {
            assert.deepEqual(profileNotices(board),
                [`Profile 1 = ${ACTION_RECALL}`, `Profile 1 = ${ACTION_CLEAR}`]);
        }

        // And the next power-on with the selector there loads nothing
        const next = playingBoard(config,
            { eeprom: board.eepromBytes(PROFILES_END), selector: POSITION_NOTE, lfo: TOP });

        assertAllEqual(play(next, 3).pitches, noteMillivolts(NOTE), 'after the power cycle');
    });

    test(`${config}: a hold of ten seconds clears every profile and leaves the rest`, () => {
        const eeprom = eepromWithProfiles({
            0: GATE_AMOUNTS, [POSITION_NOTE]: NOTE_AMOUNTS, [POSITION_CHANCE]: GATE_AMOUNTS,
            7: NOTE_AMOUNTS,
        });

        // A calibration record below them, which is the ideal table's
        eeprom.splice(0, 12, ...calRecord([0, 1000, 2000, 3000, 4000]));

        const board = playingBoard(config, { eeprom, selector: POSITION_CHANCE });

        pressButton(board, CLEAR_ALL_MS);
        board.runForMs(WRITE_MS);

        const after = board.eepromBytes(PROFILES_END);

        assert.deepEqual(after.slice(0, PROFILE_ADDRESS), eeprom.slice(0, PROFILE_ADDRESS));
        for (let profile = 0; profile < 8; profile++) {
            assert.equal(after[profileAddress(profile)], ERASED, `profile ${profile}`);
        }
        if (config === 'debug') {
            assert.deepEqual(profileNotices(board),
                [`Profile 3 = ${ACTION_RECALL}`, `Profile 3 = ${ACTION_CLEAR_ALL}`]);
        }
    });
}
