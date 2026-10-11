'use strict';
// The watchdog on the emulated board, for both firmware images. The firmware
// never stops by itself, so a scenario makes it: it switches interrupts off
// from outside, which stops the tick the watchdog is kicked from, and the
// watchdog has to notice.
//
// The emulator's watchdog restarts the program and sets the reset flag, but
// leaves every other register as it was, which a real reset does not. So
// these show that the firmware starts the watchdog, kicks it, and comes back
// from a reset it caused; not what the chip is like after one.
//
// The crash log is here too. The emulator's reset leaves the RAM as it was,
// as a real watchdog reset does, so the breadcrumb is there to be recorded:
// a fresh emulated chip's RAM is all zeros and the firmware never writes a
// place of 0, so a record that gives a place shows the byte was kept and
// was not cleared by the startup code.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard } = require('../boards/nano_sequencer');
const {
    crashRecord, NO_CRASH_RECORD, CRASH_LOG_ADDRESS, CRASH_RECORD_BYTES, CRASH_LOG_BYTES,
} = require('../boards/crash_record');

const CONFIGS = ['debug', 'release'];
// Sixteen notes, no rests, and the first few all different
const PANEL = [3, 6, 9, 12, 15, 2, 5, 8, 11, 14, 1, 4, 7, 10, 13, 2];
const READING_150_BPM = 480; // Exactly 100 ms a step
const STEP_MS = 100;
const RUN_LIMIT_MS = 5000;
const WDTCSR = 0x60;         // Data address of the watchdog's control register
const WATCHDOG_RESET_AT_250_MS = 0x0C; // Reset enable, prescaler for 256 ms
const WATCHDOG_MS = 256;
const MILLIVOLTS_PER_OCTAVE = 1000;
const SEMITONES_PER_OCTAVE = 12;
const WATCHDOG_RESET = 8;        // The reset flag, and the cause in a record
const PLACE_BETWEEN_TICKS = 5;   // The firmware's number for an idle loop
const ERASED = 0xFF;
// Longer than the five bytes of a record take to write, at 3.6 ms each
const RECORD_WRITE_MS = 25;

function noteMillivolts(noteValue) {
    return Math.round(((noteValue - 1) * MILLIVOLTS_PER_OCTAVE) / SEMITONES_PER_OCTAVE);
}

function playingBoard(config, options) {
    const board = createBoard(config, options);

    board.setNotes(PANEL);
    board.setTempoReading(READING_150_BPM);
    return board;
}

// The step each gate so far belongs to, from the voltage the DAC held as it
// rose. Only steps 0 to 4 of PANEL are asked about: their notes are unique
function stepsPlayed(board) {
    const pitches = PANEL.slice(0, 5).map(noteMillivolts);

    return board.gate.rises().map((rise) => pitches.indexOf(board.cvWriteBefore(rise).millivolts));
}

// The crash log as the EEPROM holds it, and everything below it
function crashLog(board) {
    return board.eepromBytes(CRASH_LOG_ADDRESS + CRASH_LOG_BYTES).slice(CRASH_LOG_ADDRESS);
}

function belowCrashLog(board) {
    return board.eepromBytes(CRASH_LOG_ADDRESS);
}

// A log holding these records and nothing else
function logOf(...records) {
    const log = new Array(CRASH_LOG_BYTES).fill(ERASED);

    records.forEach((record, slot) => {
        record.forEach((byte, i) => {
            log[(slot * CRASH_RECORD_BYTES) + i] = byte;
        });
    });
    return log;
}

// Stops the tick once this many gates have risen, and runs on until the
// watchdog has restarted the firmware and it has had time to write a record
function crashAfterGates(board, gates) {
    board.machine.runUntil(() => board.gate.rises().length >= gates, RUN_LIMIT_MS);
    board.stopTick();
    board.machine.runUntil(() => board.gate.rises().length >= gates + 1, RUN_LIMIT_MS);
    board.runForMs(RECORD_WRITE_MS);
}

for (const config of CONFIGS) {
    test(`${config}: playing leaves the crash log erased`, () => {
        const board = playingBoard(config);

        board.runForMs(10 * WATCHDOG_MS);

        assert.deepEqual(crashLog(board), logOf());
    });

    test(`${config}: a watchdog reset is recorded with where the loop had got to`, () => {
        const board = playingBoard(config);

        crashAfterGates(board, 3);

        // The tick was stopped with the loop still going round, so the
        // place is the one between ticks
        assert.deepEqual(crashLog(board),
            logOf(crashRecord(1, WATCHDOG_RESET, PLACE_BETWEEN_TICKS, 1)));
        assert.deepEqual(belowCrashLog(board), new Array(CRASH_LOG_ADDRESS).fill(ERASED));
    });

    test(`${config}: the same crash again raises the count, and a power cycle keeps it`, () => {
        const board = playingBoard(config);
        const twice = logOf(crashRecord(1, WATCHDOG_RESET, PLACE_BETWEEN_TICKS, 2));

        crashAfterGates(board, 3);
        crashAfterGates(board, 6);
        assert.deepEqual(crashLog(board), twice);

        // Powered up again with what the EEPROM held: not a crash, so the
        // log is read and left as it is, and the pattern plays
        const next = playingBoard(config,
            { eeprom: board.eepromBytes(CRASH_LOG_ADDRESS + CRASH_LOG_BYTES) });

        next.runForMs(2 * WATCHDOG_MS);
        assert.deepEqual(crashLog(next), twice);
        assert.deepEqual(stepsPlayed(next).slice(0, 5), [0, 1, 2, 3, 4]);
    });

    test(`${config}: the board counts the resets and reads the crash log back`, () => {
        const board = playingBoard(config);

        board.runForMs(WATCHDOG_MS);
        assert.deepEqual(board.resets, []);
        assert.deepEqual(board.crashLog(), { newest: NO_CRASH_RECORD, records: [] });

        crashAfterGates(board, 4);
        crashAfterGates(board, 7);

        assert.equal(board.resets.length, 2);
        assert.deepEqual(board.crashLog(), {
            newest: 0,
            records: [{
                slot: 0, number: 1, cause: WATCHDOG_RESET, place: PLACE_BETWEEN_TICKS, count: 2,
            }],
        });
    });

    test(`${config}: the watchdog is set to reset after 250 ms and is kept quiet`, () => {
        const board = playingBoard(config);

        // Ten times its timeout, with steps beginning all the while
        board.runForMs(10 * WATCHDOG_MS);

        assert.equal(board.machine.cpu.data[WDTCSR], WATCHDOG_RESET_AT_250_MS);
        assert.deepEqual(stepsPlayed(board).slice(0, 5), [0, 1, 2, 3, 4]);
        assert.equal(board.gate.rises().length, 26);
    });

    test(`${config}: a tick that stops is caught, and the pattern starts again`, () => {
        const board = playingBoard(config);
        const nowMs = () => board.machine.cyclesToMs(board.machine.cpu.cycles);

        // Steps 0, 1 and 2 begin; then no more interrupts, so no more ticks
        board.machine.runUntil(() => board.gate.rises().length >= 3, RUN_LIMIT_MS);
        board.stopTick();
        const stoppedMs = nowMs();

        board.machine.runUntil(() => board.gate.rises().length >= 5, RUN_LIMIT_MS);

        // The next step to begin is the first of the pattern, after a
        // watchdog timeout and not before, and the one after it is on time
        const rises = board.gate.rises().map((cycle) => board.machine.cyclesToMs(cycle));

        assert.deepEqual(stepsPlayed(board), [0, 1, 2, 0, 1]);
        assert.ok(rises[3] - stoppedMs >= WATCHDOG_MS - 2, `reset after ${rises[3] - stoppedMs} ms`);
        assert.ok(rises[3] - stoppedMs <= WATCHDOG_MS + 5, `reset after ${rises[3] - stoppedMs} ms`);
        assert.ok(Math.abs((rises[4] - rises[3]) - STEP_MS) < 0.05);
    });
}

test('debug: after a watchdog reset the start-up line gives it as the cause', () => {
    const board = playingBoard('debug');

    board.runUntilNotices(1, RUN_LIMIT_MS);
    assert.equal(board.serial.notices[0].text, 'Reset = 0 Cal = 0');

    board.runUntilLines(2, RUN_LIMIT_MS);
    board.stopTick();
    board.runUntilNotices(2, RUN_LIMIT_MS);

    assert.equal(board.serial.notices[1].text, 'Reset = 8 Cal = 0');
});

test('debug: the record reported is the one the board reads as the newest', () => {
    // A log that has gone round, its numbers going on from slot 1 to slot
    // 2 only by a gap, with one record spoiled; and one whose numbers go
    // round from 255 to 0. Neither start-up is a crash, so both are left
    const record = (number, slot) => crashRecord(number, WATCHDOG_RESET, (slot % 5) + 1, slot + 1);
    const gone = logOf(...[9, 10, 3, 4, 5, 6, 7, 8].map(record));
    const wrapped = logOf(...[254, 255, 0].map(record));
    const below = new Array(CRASH_LOG_ADDRESS).fill(ERASED);

    gone[(4 * CRASH_RECORD_BYTES) + 3] ^= 1; // Slot 4's count, not as checksummed

    const cases = [[gone, [0, 1, 2, 3, 5, 6, 7], 1], [wrapped, [0, 1, 2], 2]];

    for (const [log, slots, newest] of cases) {
        const board = playingBoard('debug', { eeprom: [...below, ...log] });
        const read = board.crashLog();
        const found = read.records.find((each) => each.slot === read.newest);

        assert.deepEqual(read.records.map((each) => each.slot), slots);
        assert.equal(read.newest, newest);
        assert.deepEqual(found, {
            slot: newest, number: log[newest * CRASH_RECORD_BYTES], cause: WATCHDOG_RESET,
            place: (newest % 5) + 1, count: newest + 1,
        });

        board.runUntilNotices(2, RUN_LIMIT_MS);
        assert.equal(board.serial.notices[1].text,
            `Crash ${found.number} Place = ${found.place} Count = ${found.count}`);
        assert.deepEqual(crashLog(board), log);
    }
});

test('debug: a crash is logged at the start-up after it and at every one after', () => {
    const board = playingBoard('debug');

    // Nothing in the log: the start-up line is alone, ahead of the steps
    board.runUntilLines(2, RUN_LIMIT_MS);
    assert.deepEqual(board.serial.notices.map((notice) => notice.text), ['Reset = 0 Cal = 0']);

    board.stopTick();
    board.runUntilNotices(3, RUN_LIMIT_MS);
    assert.equal(board.serial.notices[1].text, 'Reset = 8 Cal = 0');
    assert.equal(board.serial.notices[2].text, 'Crash 1 Place = 5 Count = 1');

    // After a power cycle the reset is no longer the watchdog, and the
    // record is still reported
    board.runForMs(RECORD_WRITE_MS);
    const next = playingBoard('debug',
        { eeprom: board.eepromBytes(CRASH_LOG_ADDRESS + CRASH_LOG_BYTES) });

    next.runUntilLines(1, RUN_LIMIT_MS);
    assert.deepEqual(next.serial.notices.map((notice) => notice.text),
        ['Reset = 0 Cal = 0', 'Crash 1 Place = 5 Count = 1']);
});
