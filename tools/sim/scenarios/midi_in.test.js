'use strict';
// MIDI in on the emulated board: the MIDI clock as a clock source, and
// Start, Stop and Continue. The bytes come from parts/midi_source.js, each
// handed to the chip's USART whole as its last bit would arrive, 320 us
// after the one before. What that leaves out: the pin, the optocoupler in
// front of it, a wrong baud rate and a damaged byte, none of which the
// model can show.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard } = require('../boards/nano_sequencer');
const { STEP_MS, assertNear } = require('./fixture');
const {
    READING_150_BPM, READING_ONE_TO_ONE, READING_MULTIPLY_4, PANEL,
    panelNote, midiBoard, ms, ofType, withoutClocks, assertBetween,
    assertWellFormed, soundingAtEnd,
} = require('./midi_fixture');

const START = 0xFA;
const CONTINUE = 0xFB;
const STOP = 0xFC;
const CLOCK = 0xF8;
const ACTIVE_SENSING = 0xFE;
const NOTE_ON = [0x90, 0x3C, 0x64];
const NOTE_OFF = [0x80, 0x3C, 0x00];
const CLOCKS_PER_EDGE = 6;       // Six MIDI clocks are one edge: a step
const CLOCK_IN_MS = 20;          // The MIDI clock sent in: a step every 120 ms
const EDGE_MS = CLOCK_IN_MS * CLOCKS_PER_EDGE;
const READING_DIVIDE_4 = 0;      // A step to every four edges
const INTERNAL_MS = 150;         // Long enough for steps 0 and 1 at 150 BPM
// A clock byte is read on the tick after it arrives and its step begins on
// the tick after that: 1.88 ms measured, and it can be 1.2 to 2.2
const STEP_AFTER_CLOCK_MIN_MS = 1.0;
const STEP_AFTER_CLOCK_MAX_MS = 2.4;
// Steps on a MIDI clock whose period is a whole number of ticks land on the
// same point of a tick each time
const STEP_JITTER_MAX_MS = 0.05;
const RUN_HOLD_MS = 60;

// The clock output's rises from the nth on, in milliseconds
function stepsFrom(board, first) {
    return board.clockOut.rises().slice(first).map((cycle) => ms(board, cycle));
}

// The notes sent from the nth Note On on, as steps of PANEL would give them
function notesFrom(board, first) {
    return ofType(board, 'noteOn').slice(first).map((message) => message.note);
}

function panelNotes(firstStep, count) {
    return Array.from({ length: count },
        (unused, i) => panelNote(PANEL[(firstStep + i) % PANEL.length]));
}

test('midi in: a MIDI clock takes the pattern over at six clocks a step', () => {
    const board = midiBoard(PANEL, READING_ONE_TO_ONE);
    const steps = 12;

    // Steps 0 and 1 on the internal clock
    board.runForMs(INTERNAL_MS);
    assert.equal(board.clockOut.rises().length, 2);

    // Clocks for twelve steps, the first of them an edge, and five more
    board.startMidiClockIn(CLOCK_IN_MS);
    board.runForMs((steps * EDGE_MS) - (CLOCK_IN_MS / 2));
    board.stopMidiClockIn();
    assert.equal(board.midiIn.sent.length, steps * CLOCKS_PER_EDGE);
    // With no clocks the pattern waits: nothing more begins
    board.runForMs(10 * EDGE_MS);

    const rises = stepsFrom(board, 2);
    const firstClockMs = ms(board, board.midiIn.sent[0].cycle);

    assert.equal(rises.length, steps);
    assertBetween(rises[0] - firstClockMs, STEP_AFTER_CLOCK_MIN_MS,
        STEP_AFTER_CLOCK_MAX_MS, 'first step after the first clock');
    for (let i = 1; i < rises.length; i++) {
        assertNear(rises[i] - rises[i - 1], EDGE_MS, STEP_JITTER_MAX_MS, `step ${i}`);
    }
    // No restart: the pattern goes on from where the internal clock left it
    assert.deepEqual(notesFrom(board, 0), panelNotes(0, 2 + steps));
    assertWellFormed(board);
});

test('midi in: the pot is the ratio it is on the clock input', () => {
    const edges = 5;

    for (const [reading, steps] of [[READING_MULTIPLY_4, 1 + (4 * (edges - 1))],
        [READING_DIVIDE_4, 2]]) {
        const board = midiBoard(PANEL, reading);

        board.runForMs(INTERNAL_MS);
        const before = board.clockOut.rises().length;

        // Five edges' worth of clocks; the first stretch is always one step
        board.startMidiClockIn(CLOCK_IN_MS);
        board.runForMs((edges * EDGE_MS) - (CLOCK_IN_MS / 2));
        board.stopMidiClockIn();
        board.runForMs(10 * EDGE_MS);

        assert.equal(board.midiIn.sent.length, edges * CLOCKS_PER_EDGE);
        assert.equal(board.clockOut.rises().length - before, steps, `reading ${reading}`);
    }
});

test('midi in: no clock is lost at the most the line can carry', () => {
    // 240 clocks with no gap between them are 40 edges, and at a step to
    // four edges that is ten steps, the first on the first edge
    const divided = midiBoard(PANEL, READING_DIVIDE_4);

    divided.runForMs(INTERNAL_MS);
    const before = divided.clockOut.rises().length;

    divided.sendMidi(new Array(240).fill(CLOCK));
    divided.runForMs(600);
    assert.equal(divided.midiIn.sent.length, 240);
    assert.equal(divided.clockOut.rises().length - before, 10);

    // And with nine other bytes after each clock, 120 clocks are 20 steps
    const mixed = midiBoard(PANEL, READING_ONE_TO_ONE);
    const bytes = [];

    for (let i = 0; i < 120; i++) {
        bytes.push(CLOCK, ACTIVE_SENSING, ...NOTE_ON, ...NOTE_OFF, ACTIVE_SENSING,
            ACTIVE_SENSING);
    }
    mixed.runForMs(INTERNAL_MS);
    mixed.sendMidi(bytes);
    mixed.runForMs(1000);
    assert.equal(mixed.midiIn.sent.length, bytes.length);
    assert.equal(mixed.clockOut.rises().length, 2 + 20);
    assertWellFormed(mixed);
});

for (const [name, startByte] of [['Start', START], ['Continue', CONTINUE]]) {
    test(`midi in: a Stop stops the pattern and a ${name} plays it from step 0`, () => {
        const board = midiBoard(PANEL, READING_ONE_TO_ONE);

        // Steps 0 and 1 at the tempo, then 2, 3 and 4 on the MIDI clock
        board.runForMs(INTERNAL_MS);
        board.startMidiClockIn(CLOCK_IN_MS);
        board.runForMs((3 * EDGE_MS) - CLOCK_IN_MS);
        assert.equal(board.clockOut.rises().length, 5);

        // Stopped: the clocks keep coming and nothing begins
        board.sendMidi([STOP]);
        board.runForMs((2 * EDGE_MS) + 5);
        assert.equal(board.clockOut.rises().length, 5);
        assert.equal(board.gate.edges[board.gate.edges.length - 1].high, false);

        // The clock after the Start is the first step, wherever the count
        // of clocks stood
        board.sendMidi([startByte]);
        const startMs = ms(board, board.machine.cpu.cycles);

        board.runForMs((3 * EDGE_MS) + 40);
        board.stopMidiClockIn();
        board.runForMs(5 * EDGE_MS);

        const rises = stepsFrom(board, 5);
        const nextClock = board.midiIn.sent.find(
            (byte) => (byte.value === CLOCK) && (ms(board, byte.cycle) > startMs + 0.5));

        assert.ok(rises.length >= 3, `${rises.length} steps after the ${name}`);
        assertBetween(rises[0] - ms(board, nextClock.cycle), STEP_AFTER_CLOCK_MIN_MS,
            STEP_AFTER_CLOCK_MAX_MS, 'first step after the clock that follows');
        assertNear(rises[1] - rises[0], EDGE_MS, STEP_JITTER_MAX_MS, 'second step');
        assert.deepEqual(notesFrom(board, 5).slice(0, 3), panelNotes(0, 3));

        // MIDI out says the same: a Stop, then a Start, and no note left on
        assert.deepEqual(
            withoutClocks(board.midi.messages).filter((type) => /^st/.test(type)),
            ['start', 'stop', 'start']);
        assert.deepEqual(soundingAtEnd(board).length <= 1, true);
        assertWellFormed(board);
    });
}

test('midi in: the first external clock since the last stop is the one followed', () => {
    const board = midiBoard(PANEL, READING_ONE_TO_ONE);

    // The clock input first, at a step every 100 ms; a MIDI clock that
    // would make a step every 42 ms comes after it and moves nothing
    board.runForMs(INTERNAL_MS);
    board.startClockIn(STEP_MS);
    board.runForMs(30);
    board.startMidiClockIn(7);
    board.runForMs(970);

    let rises = stepsFrom(board, 2);

    assert.equal(rises.length, 10);
    for (let i = 1; i < rises.length; i++) {
        assertNear(rises[i] - rises[i - 1], STEP_MS, 1, `step ${i} on the clock input`);
    }

    // A stop forgets it. Then the MIDI clock first, and the clock input
    // after it is the one passed over
    board.stopClockIn();
    board.stopMidiClockIn();
    board.runForMs(20);
    board.pressRunButton(RUN_HOLD_MS);
    board.runForMs(RUN_HOLD_MS);
    const stopped = board.clockOut.rises().length;

    // Six clocks, so that one of them is an edge wherever the count stood
    board.startMidiClockIn(CLOCK_IN_MS);
    board.runForMs(EDGE_MS + (CLOCK_IN_MS / 2));
    board.startClockIn(37);
    board.pressRunButton(RUN_HOLD_MS);
    board.runForMs(1000);
    assert.equal(board.clockOut.rises().length > stopped, true);

    rises = stepsFrom(board, stopped);
    for (let i = 1; i < rises.length; i++) {
        assertNear(rises[i] - rises[i - 1], EDGE_MS, STEP_JITTER_MAX_MS,
            `step ${i} on the MIDI clock`);
    }
});

test('midi in: notes and other bytes change nothing', () => {
    const quiet = midiBoard(PANEL, READING_150_BPM);
    const spoken = midiBoard(PANEL, READING_150_BPM);

    for (let i = 0; i < 20; i++) {
        spoken.sendMidi((i % 2 === 0) ? NOTE_ON : NOTE_OFF);
        spoken.sendMidi([ACTIVE_SENSING]);
        spoken.runForMs(STEP_MS / 2);
        quiet.runForMs(STEP_MS / 2);
    }

    const quietRises = stepsFrom(quiet, 0);
    const spokenRises = stepsFrom(spoken, 0);

    assert.equal(spoken.midiIn.sent.length, 80);
    assert.equal(spokenRises.length, quietRises.length);
    for (let i = 0; i < quietRises.length; i++) {
        // Taking a byte in is a few microseconds of the loop's time
        assertNear(spokenRises[i], quietRises[i], 0.05, `step ${i}`);
    }
    assert.deepEqual(withoutClocks(spoken.midi.messages), withoutClocks(quiet.midi.messages));
});

for (const config of ['debug', 'release']) {
    test(`${config}: MIDI in is not listened to`, () => {
        const board = createBoard(config);

        board.setNotes(PANEL);
        board.setTempoReading(READING_150_BPM);
        board.runForMs(INTERNAL_MS);
        board.startMidiClockIn(7);
        board.sendMidi([STOP]);
        board.runForMs(850);

        const rises = stepsFrom(board, 0);

        // Ten steps at the tempo, whatever was sent
        assert.equal(rises.length, 10);
        for (let i = 1; i < rises.length; i++) {
            assertNear(rises[i] - rises[i - 1], STEP_MS, 1, `step ${i}`);
        }
    });
}
