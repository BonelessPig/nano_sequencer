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
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard } = require('../boards/nano_sequencer');

const CONFIGS = ['debug', 'release'];
// Sixteen notes, no rests, and the first few all different
const PANEL = [3, 6, 9, 12, 15, 2, 5, 8, 11, 14, 1, 4, 7, 10, 13, 2];
const READING_150_BPM = 480; // Exactly 100 ms a step
const STEP_MS = 100;
const RUN_LIMIT_MS = 5000;
const WDTCSR = 0x60;         // Data addresses of the registers used
const SREG = 0x5F;
const INTERRUPT_ENABLE = 0x80;
const WATCHDOG_RESET_AT_250_MS = 0x0C; // Reset enable, prescaler for 256 ms
const WATCHDOG_MS = 256;
const MILLIVOLTS_PER_OCTAVE = 1000;
const SEMITONES_PER_OCTAVE = 12;

function noteMillivolts(noteValue) {
    return Math.round(((noteValue - 1) * MILLIVOLTS_PER_OCTAVE) / SEMITONES_PER_OCTAVE);
}

function playingBoard(config) {
    const board = createBoard(config);

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

for (const config of CONFIGS) {
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
        board.machine.cpu.data[SREG] &= ~INTERRUPT_ENABLE;
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
    board.machine.cpu.data[SREG] &= ~INTERRUPT_ENABLE;
    board.runUntilNotices(2, RUN_LIMIT_MS);

    assert.equal(board.serial.notices[1].text, 'Reset = 8 Cal = 0');
});
