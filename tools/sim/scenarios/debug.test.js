'use strict';
// The debug firmware image on the emulated board: what it logs, how it
// drives the shift registers, and how long a step takes.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard, STEP_COUNT } = require('../boards/nano_sequencer');

const PANEL = [1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2, 5, 8, 11, 14];
const CLOCKS_PER_READ = 64;    // 16 steps of 4 bits
const CLOCKS_PER_CV_WRITE = 16; // One DAC frame, sent on every tick
const SCAN_PERIOD_TICKS = 8;
const SCAN_PERIOD_MS = 8;      // The panel and pot are read every 8 ticks
// On a tick that also queues a log line the read starts about 0.11 ms later
const SCAN_JITTER_MAX_MS = 0.2;
const FASTEST_READING = 1023;  // 285 BPM, about 52.6 ms a step
const READING_150_BPM = 480;   // Exactly 100 ms a step
const STEP_MS_150_BPM = 100;
const READING_30_BPM = 0;      // The slowest tempo: 500 ms a step
const STEP_MS_30_BPM = 500;
const TICK_MS = 1;
// A step's log line starts within this long of its tick boundary: the time
// to queue the line, plus a panel and pot read if that tick has one
const LOG_START_JITTER_MAX_MS = 0.5;
// The gate and the clock output are set first in a tick, ahead of the log
const OUTPUT_JITTER_MAX_MS = 0.02;
const RUN_LIMIT_MS = 10000;

function debugBoard(tempoReading) {
    const board = createBoard('debug');
    board.setNotes(PANEL);
    board.setTempoReading(tempoReading);
    return board;
}

function assertNear(actual, expected, tolerance, what) {
    assert.ok(Math.abs(actual - expected) <= tolerance,
        `${what}: ${actual} ms, expected ${expected} within ${tolerance}`);
}

// The firmware plays the chromatic scale from the lowest pitch: a note value
// of 0 is a rest, and any other is logged as its pitch, one less
function expectedLine(step, noteValue) {
    return (noteValue === 0)
        ? `Step ${step} Rest`
        : `Step ${step} Note = ${noteValue - 1}`;
}

test('logs every step with the pitch set on the panel or as a rest, and wraps', () => {
    const board = debugBoard(FASTEST_READING);
    const lineCount = (STEP_COUNT * 2) + 1;

    board.runUntilLines(lineCount, RUN_LIMIT_MS);

    board.serial.lines.forEach((line, i) => {
        const step = i % STEP_COUNT;
        assert.equal(line.text, expectedLine(step, PANEL[step]));
    });
    assert.equal(board.serial.lines[5].text, 'Step 5 Rest');
    assert.equal(board.serial.lines[10].text, 'Step 10 Note = 14');
});

test('sends at 115200 baud in double-speed mode', () => {
    const board = debugBoard(FASTEST_READING);

    board.runUntilLines(1, RUN_LIMIT_MS);

    assert.equal(board.machine.usart.txEnable, true);
    assert.equal(Math.round(board.machine.usart.baudRate), 117647);
});

test('reads the panel every 8 ms, with 64 clock pulses', () => {
    const board = debugBoard(FASTEST_READING);
    const readCount = 200;

    board.runUntilPanelReads(readCount + 1, RUN_LIMIT_MS);

    // 64 for the read itself; the chain is also clocked by the eight DAC
    // frames sent before the next load, having no chip select of its own
    for (const read of board.panelReads.slice(0, readCount)) {
        assert.equal(read.clocks,
            CLOCKS_PER_READ + (SCAN_PERIOD_TICKS * CLOCKS_PER_CV_WRITE));
    }
    for (const periodMs of board.panelReadPeriodsMs()) {
        assertNear(periodMs, SCAN_PERIOD_MS, SCAN_JITTER_MAX_MS, 'panel read period');
    }
});

test('a switch changed mid-pattern is heard when its step next plays', () => {
    const board = debugBoard(FASTEST_READING);
    const changed = PANEL.slice();
    changed[2] = 9;

    board.runUntilLines(STEP_COUNT, RUN_LIMIT_MS);
    board.setNotes(changed);
    board.runUntilLines(STEP_COUNT * 2, RUN_LIMIT_MS);

    assert.equal(board.serial.lines[2].text, expectedLine(2, PANEL[2]));
    assert.equal(board.serial.lines[STEP_COUNT + 2].text, expectedLine(2, 9));
});

test('a step lasts 100 ms at 150 BPM', () => {
    const board = debugBoard(READING_150_BPM);
    const lineCount = STEP_COUNT + 2;

    board.runUntilLines(lineCount, RUN_LIMIT_MS);

    const periods = board.lineStartPeriodsMs();
    // The first tick begins step 0 and counts towards step 1
    assertNear(periods[0], STEP_MS_150_BPM - TICK_MS, LOG_START_JITTER_MAX_MS, 'first step');
    periods.slice(1).forEach((periodMs, i) => {
        assertNear(periodMs, STEP_MS_150_BPM, LOG_START_JITTER_MAX_MS, `step ${i + 1}`);
    });
    // No drift: the whole run is as long as its steps, not that plus overhead
    const lines = board.serial.lines;
    const runMs = board.machine.cyclesToMs(lines[lineCount - 1].startCycle - lines[1].startCycle);
    assertNear(runMs, STEP_MS_150_BPM * (lineCount - 2), LOG_START_JITTER_MAX_MS, 'whole run');
});

test('a step lasts 500 ms with the tempo pot at zero', () => {
    const board = debugBoard(READING_30_BPM);

    board.runUntilLines(4, RUN_LIMIT_MS);

    const periods = board.lineStartPeriodsMs();
    assertNear(periods[1], STEP_MS_30_BPM, LOG_START_JITTER_MAX_MS, 'second step');
    assertNear(periods[2], STEP_MS_30_BPM, LOG_START_JITTER_MAX_MS, 'third step');
});

test('the gate opens before its step is logged, and not for a rest', () => {
    const board = debugBoard(READING_150_BPM);

    board.runUntilLines(STEP_COUNT + 1, RUN_LIMIT_MS);

    // Every line but the rest's has a gate, which rose in the same tick as
    // the line's first byte went out, and ahead of it
    const rises = board.gate.rises();
    const noteLines = board.serial.lines.filter((line) => !line.text.endsWith('Rest'));
    assert.equal(noteLines.length, STEP_COUNT);
    assert.equal(rises.length, noteLines.length);
    noteLines.forEach((line, i) => {
        const leadMs = board.machine.cyclesToMs(line.startCycle - rises[i]);
        assert.ok((leadMs > 0) && (leadMs < LOG_START_JITTER_MAX_MS),
            `${line.text}: gate led the log by ${leadMs} ms`);
    });
});

test('the pitch CV matches the pitch logged for each note', () => {
    const board = debugBoard(READING_150_BPM);

    board.runUntilLines(STEP_COUNT + 1, RUN_LIMIT_MS);

    // A logged pitch is in semitones, and the DAC is at 1 V to the octave.
    // The pitch is written ahead of the gate, and the gate ahead of the log
    const rises = board.gate.rises();
    const noteLines = board.serial.lines.filter((line) => !line.text.endsWith('Rest'));
    assert.equal(board.cvIgnoredFrames.length, 0);
    noteLines.forEach((line, i) => {
        const semitone = Number(line.text.split(' = ')[1]);
        const write = board.cvWriteBefore(rises[i]);

        assert.equal(write.millivolts, Math.round((semitone * 1000) / 12), line.text);
    });
});

test('the gate keeps time while the log is being sent', () => {
    const board = debugBoard(READING_150_BPM);

    board.runUntilClockOutPulses(STEP_COUNT + 1, RUN_LIMIT_MS);

    // The outputs are set before anything else in a tick, so the log costs
    // them nothing: the same figures as the release build
    const pulses = board.clockOutPulsesMs();
    for (let i = 1; i < STEP_COUNT; i++) {
        assertNear(pulses[i].widthMs, STEP_MS_150_BPM / 2, OUTPUT_JITTER_MAX_MS, `width ${i}`);
        assertNear(pulses[i + 1].riseMs - pulses[i].riseMs, STEP_MS_150_BPM,
            OUTPUT_JITTER_MAX_MS, `period ${i}`);
    }
    for (const gate of board.gatePulsesMs().slice(1)) {
        assertNear(gate.widthMs, STEP_MS_150_BPM / 2, OUTPUT_JITTER_MAX_MS, 'gate width');
    }
});

test('the first step is logged within 3 ms of reset', () => {
    const board = debugBoard(READING_150_BPM);

    board.runUntilLines(1, RUN_LIMIT_MS);

    const startMs = board.machine.cyclesToMs(board.serial.lines[0].startCycle);
    assert.ok(startMs < 3, `first line started at ${startMs} ms`);
});
