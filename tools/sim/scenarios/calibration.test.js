'use strict';
// Pitch calibration on the emulated board, for all three firmware images:
// the record in the EEPROM, what is played from it, and calibration mode.
// The release and midi images have no log, so everything is checked on the
// DAC, the gate and the EEPROM's contents; the debug image's log lines are
// checked as well.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard, STEP_COUNT } = require('../boards/nano_sequencer');
const { calRecord } = require('../boards/cal_record');

const CONFIGS = ['debug', 'release', 'midi'];
const PANEL = [1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2, 5, 8, 11, 14];
const READING_150_BPM = 480;     // Exactly 100 ms a step
const RUN_LIMIT_MS = 5000;
const RECORD_BYTES = 12;
const ERASED = 0xFF;
const IDEAL_TABLE = [0, 1000, 2000, 3000, 4000];
// Every octave a little out, each by a different amount
const STORED_TABLE = [10, 1100, 2000, 3050, 4000];
const SEMITONES_PER_OCTAVE = 12;
const TRIM_CENTRE_READING = 512; // The pot position that trims nothing
const TRIM_13_UP_READING = 616;  // 8 counts of the reading to one millivolt
const TICK_MS = 1;
const SETTLE_MS = 20;            // More than two panel scans
const CONFIRM_MS = 250;          // The gate is low this long after a store
const EDGE_JITTER_MAX_MS = 0.02;
// From reset to the timer being started, and the first tick's work done:
// 0.52 ms was measured on the debug image
const START_UP_MAX_MS = 0.6;
const POWER_ON_RESET = 0x01;
const EXTERNAL_RESET = 0x02;

function assertNear(actual, expected, tolerance, what) {
    assert.ok(Math.abs(actual - expected) <= tolerance,
        `${what}: ${actual} ms, expected ${expected} within ${tolerance}`);
}

// What the firmware should send the DAC for a note value through a table:
// the chromatic scale from the lowest pitch, on the straight line between
// the two octave entries either side, rounded to the nearest millivolt
function noteMillivolts(table, noteValue) {
    const semitone = noteValue - 1;
    const point = Math.floor(semitone / SEMITONES_PER_OCTAVE);
    const above = semitone % SEMITONES_PER_OCTAVE;
    const weighted = (table[point] * (SEMITONES_PER_OCTAVE - above)) + (table[point + 1] * above);

    return Math.floor((weighted + (SEMITONES_PER_OCTAVE / 2)) / SEMITONES_PER_OCTAVE);
}

// One pass of PANEL at 150 BPM and a little of the next
function playedBoard(config, options) {
    const board = createBoard(config, options);

    board.setNotes(PANEL);
    board.setTempoReading(READING_150_BPM);
    board.runUntilClockOutPulses(STEP_COUNT + 1, RUN_LIMIT_MS);
    return board;
}

// Checks the voltage the DAC held as each gate rose against a table
function assertPlayedFrom(board, table) {
    const noteSteps = PANEL.map((note, step) => step).filter((step) => PANEL[step] !== 0);
    const rises = board.gate.rises();

    noteSteps.forEach((step, i) => {
        assert.equal(board.cvWriteBefore(rises[i]).millivolts,
            noteMillivolts(table, PANEL[step]), `step ${step} pitch`);
    });
}

// A board powered up in calibration mode and left to settle
function calibrationBoard(config, point, tempoReading, options) {
    const board = createBoard(config, options);

    board.setCalibrationPanel(point, true);
    board.setTempoReading(tempoReading);
    board.runForMs(SETTLE_MS);
    return board;
}

// Switches the store switch off and on again, as someone at the panel would
function storePoint(board, point) {
    board.setCalibrationPanel(point, false);
    board.runForMs(SETTLE_MS);
    board.setCalibrationPanel(point, true);
    board.runForMs(SETTLE_MS);
}

function lastCvMillivolts(board) {
    return board.cvWrites[board.cvWrites.length - 1].millivolts;
}

for (const config of CONFIGS) {
    test(`${config}: plays the ideal table and leaves an erased EEPROM alone`, () => {
        const board = playedBoard(config);

        assertPlayedFrom(board, IDEAL_TABLE);
        assert.deepEqual(board.eepromBytes(RECORD_BYTES + 4),
            new Array(RECORD_BYTES + 4).fill(ERASED));
    });

    test(`${config}: plays from a calibration stored in the EEPROM`, () => {
        const record = calRecord(STORED_TABLE);
        const board = playedBoard(config, { eeprom: record });

        // Note value 1 is the first entry itself, and note value 15 is two
        // semitones into the second octave: 1100 + (900 * 2 / 12)
        assert.equal(noteMillivolts(STORED_TABLE, 1), 10);
        assert.equal(noteMillivolts(STORED_TABLE, 15), 1250);
        assertPlayedFrom(board, STORED_TABLE);
        assert.deepEqual(board.eepromBytes(RECORD_BYTES), record); // Read, not rewritten
    });

    test(`${config}: a record with one bit wrong leaves the ideal table`, () => {
        const record = calRecord(STORED_TABLE);

        record[3] ^= 0x01;
        assertPlayedFrom(playedBoard(config, { eeprom: record }), IDEAL_TABLE);
    });

    test(`${config}: a record of another version leaves the ideal table`, () => {
        const record = calRecord(STORED_TABLE);

        record[0] = 2;
        assertPlayedFrom(playedBoard(config, { eeprom: record }), IDEAL_TABLE);
    });

    test(`${config}: the entry pattern at power-on holds one point for the meter`, () => {
        const board = calibrationBoard(config, 2, TRIM_CENTRE_READING);

        // Chosen on the first tick and applied on the second: the nominal
        // voltage of point 2 with the gate high, and nothing sequenced
        assert.equal(lastCvMillivolts(board), 2000);
        assert.equal(board.gate.rises().length, 1);
        // (the debug image starts its timer about 0.3 ms after reset)
        assertNear(board.machine.cyclesToMs(board.gate.rises()[0]), 2 * TICK_MS,
            START_UP_MAX_MS, 'gate rise');
        assert.equal(board.gate.edges.length, 1);
        assert.equal(board.clockOut.edges.length, 0);
        assert.equal(board.serial.lines.length, 0);

        // The pot trims it and step 0's switches move it to another point
        board.setTempoReading(TRIM_13_UP_READING);
        board.runForMs(SETTLE_MS);
        assert.equal(lastCvMillivolts(board), 2013);
        board.setCalibrationPanel(4, true);
        board.setTempoReading(0);
        board.runForMs(SETTLE_MS);
        assert.equal(lastCvMillivolts(board), 3936);
        board.setCalibrationPanel(0, true);
        board.runForMs(SETTLE_MS);
        assert.equal(lastCvMillivolts(board), 0); // Cannot go below 0 V

        // Still one frame a tick, and nothing stored without the switch
        assert.equal(board.cvIgnoredFrames.length, 0);
        assert.deepEqual(board.eepromBytes(RECORD_BYTES), new Array(RECORD_BYTES).fill(ERASED));
    });

    test(`${config}: the store switch writes the record and drops the gate`, () => {
        const board = calibrationBoard(config, 2, TRIM_13_UP_READING);

        storePoint(board, 2);
        board.runForMs(CONFIRM_MS + SETTLE_MS);

        // The whole table: the stored point, the others still ideal
        assert.deepEqual(board.eepromBytes(RECORD_BYTES), calRecord([0, 1000, 2013, 3000, 4000]));
        assert.equal(board.eepromBytes(RECORD_BYTES + 1)[RECORD_BYTES], ERASED);

        // The gate went low once, for a quarter of a second, and came back
        const edges = board.gate.edges;
        assert.equal(edges.length, 3);
        assert.deepEqual(edges.map((edge) => edge.high), [true, false, true]);
        assertNear(board.machine.cyclesToMs(edges[2].cycle - edges[1].cycle), CONFIRM_MS,
            EDGE_JITTER_MAX_MS, 'gate low time');

        // The pitch did not move, and the DAC was written once a tick right
        // through the 40 ms or so that the EEPROM took
        const writes = board.cvWrites;
        assert.equal(board.cvIgnoredFrames.length, 0);
        for (let i = 2; i < writes.length; i++) {
            const periodMs = board.machine.cyclesToMs(writes[i].cycle - writes[i - 1].cycle);

            assert.equal(writes[i].millivolts, 2013);
            assertNear(periodMs, TICK_MS, EDGE_JITTER_MAX_MS, `write ${i} period`);
        }
    });

    test(`${config}: stored points are played after the power is cycled`, () => {
        const calibrating = calibrationBoard(config, 1, TRIM_13_UP_READING);

        storePoint(calibrating, 1);
        calibrating.setTempoReading(0);
        calibrating.setCalibrationPanel(0, true);
        calibrating.runForMs(CONFIRM_MS);
        storePoint(calibrating, 0);
        calibrating.setTempoReading(TRIM_CENTRE_READING);
        calibrating.setCalibrationPanel(2, true);
        calibrating.runForMs(CONFIRM_MS);
        storePoint(calibrating, 2);
        calibrating.runForMs(CONFIRM_MS);

        // Point 0 cannot be trimmed below 0 V
        const table = [0, 1013, 2000, 3000, 4000];
        const stored = calibrating.eepromBytes(RECORD_BYTES);
        assert.deepEqual(stored, calRecord(table));

        // The same EEPROM in a board powered up with an ordinary panel
        const playing = playedBoard(config, { eeprom: stored });
        assertPlayedFrom(playing, table);
        assert.equal(noteMillivolts(table, 13), 1013);
        assert.deepEqual(playing.eepromBytes(RECORD_BYTES), stored);
    });

    test(`${config}: storing over a loaded calibration keeps its other points`, () => {
        const board = calibrationBoard(config, 3, TRIM_CENTRE_READING,
            { eeprom: calRecord(STORED_TABLE) });

        storePoint(board, 3);
        board.runForMs(CONFIRM_MS);

        assert.deepEqual(board.eepromBytes(RECORD_BYTES), calRecord([10, 1100, 2000, 3000, 4000]));
    });

    test(`${config}: the entry pattern set after power-on is only a pattern`, () => {
        const board = createBoard(config);

        board.setNotes(PANEL);
        board.setTempoReading(READING_150_BPM);
        board.runUntilClockOutPulses(2, RUN_LIMIT_MS);
        storePoint(board, 2);
        board.runUntilClockOutPulses(6, RUN_LIMIT_MS);

        assert.deepEqual(board.eepromBytes(RECORD_BYTES), new Array(RECORD_BYTES).fill(ERASED));
    });
}

test('debug: logs the reset cause and that no calibration is stored', () => {
    const board = createBoard('debug', { resetFlags: EXTERNAL_RESET });

    board.setNotes(PANEL);
    board.setTempoReading(READING_150_BPM);
    board.runUntilLines(1, RUN_LIMIT_MS);

    // The first line out, ahead of the first step
    assert.equal(board.serial.notices.length, 1);
    assert.equal(board.serial.notices[0].text, 'Reset = 2 Cal = 0');
    assert.ok(board.serial.notices[0].cycle < board.serial.lines[0].startCycle);
});

test('debug: logs a power-on reset and a stored calibration', () => {
    const board = createBoard('debug',
        { resetFlags: POWER_ON_RESET, eeprom: calRecord(STORED_TABLE) });

    board.setNotes(PANEL);
    board.runUntilNotices(1, RUN_LIMIT_MS);

    assert.equal(board.serial.notices[0].text, 'Reset = 1 Cal = 1');
});

test('debug: logs each point as it is stored', () => {
    const board = calibrationBoard('debug', 2, TRIM_13_UP_READING);

    storePoint(board, 2);
    board.runUntilNotices(2, RUN_LIMIT_MS);

    assert.equal(board.serial.notices[1].text, 'Cal point 2 = 2013');
    assert.equal(board.serial.lines.length, 0);
});
