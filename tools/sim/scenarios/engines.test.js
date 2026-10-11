'use strict';
// The mode switches and the engines they choose, on the emulated board, for
// all three firmware images. The release and midi images have no log, so
// steps are timed from the clock output and told apart by the voltage the
// DAC holds as each gate rises; the debug image's log lines are checked as
// well, and the midi image's MIDI in midi.test.js.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard } = require('../boards/nano_sequencer');

const CONFIGS = ['debug', 'release', 'midi'];
const MODE_PLAIN = 0;
const MODE_STAGE = 1;
const READING_150_BPM = 480; // Exactly 100 ms a step
const STEP_MS = 100;
const HALF_STEP_MS = 50;
const RUN_LIMIT_MS = 5000;
const EDGE_JITTER_MAX_MS = 0.02;
const MILLIVOLTS_PER_OCTAVE = 1000;
const SEMITONES_PER_OCTAVE = 12;

// A stage's control: its gate mode in the high two switches, its length in
// steps, less one, in the low two
const SINGLE = 0;
const REPEAT = 1;
const HOLD = 2;
const REST = 3;
const control = (mode, length) => (mode << 2) | (length - 1);

// Eight stages with a note each, then their controls. Stage 0 holds for
// three steps, stage 1 repeats over two, stage 2 plays the first of two,
// stage 3 is a rest, and the last four are a step each: twelve steps a cycle
const STAGE_NOTES = [3, 6, 9, 12, 15, 2, 5, 8];
const STAGE_PANEL = STAGE_NOTES.concat([
    control(HOLD, 3), control(REPEAT, 2), control(SINGLE, 2), control(REST, 1),
    0, 0, 0, 0,
]);
const CYCLE_STEPS = 12;
// The gates of one cycle: the step each rises on, how long it is, and the
// stage whose note it plays
const STAGE_GATES = [
    { step: 0, widthMs: (2 * STEP_MS) + HALF_STEP_MS, stage: 0 },
    { step: 3, widthMs: HALF_STEP_MS, stage: 1 },
    { step: 4, widthMs: HALF_STEP_MS, stage: 1 },
    { step: 5, widthMs: HALF_STEP_MS, stage: 2 },
    { step: 8, widthMs: HALF_STEP_MS, stage: 4 },
    { step: 9, widthMs: HALF_STEP_MS, stage: 5 },
    { step: 10, widthMs: HALF_STEP_MS, stage: 6 },
    { step: 11, widthMs: HALF_STEP_MS, stage: 7 },
];

function assertNear(actual, expected, tolerance, what) {
    assert.ok(Math.abs(actual - expected) <= tolerance,
        `${what}: ${actual} ms, expected ${expected} within ${tolerance}`);
}

function noteMillivolts(noteValue) {
    return Math.round(((noteValue - 1) * MILLIVOLTS_PER_OCTAVE) / SEMITONES_PER_OCTAVE);
}

function boardWith(config, modeSwitches) {
    const board = createBoard(config);

    board.setNotes(STAGE_PANEL);
    board.setModeSwitches(modeSwitches);
    board.setTempoReading(READING_150_BPM);
    return board;
}

function runUntilGates(board, count) {
    board.machine.runUntil(() => board.gate.pulses().length >= count, RUN_LIMIT_MS);
}

for (const config of CONFIGS) {
    test(`${config}: the stage engine gives each stage its length and gate mode`, () => {
        const board = boardWith(config, MODE_STAGE);

        // One cycle, and the first gate of the next
        runUntilGates(board, STAGE_GATES.length + 1);

        const gates = board.gatePulsesMs();
        const clocks = board.clockOutPulsesMs();
        const rises = board.gate.rises();

        STAGE_GATES.forEach((expected, i) => {
            const what = `gate ${i}`;

            // It rises with the clock pulse of its step and plays its
            // stage's note
            assertNear(gates[i].riseMs, clocks[expected.step].riseMs, 0.001, `${what} rise`);
            assertNear(gates[i].widthMs, expected.widthMs, EDGE_JITTER_MAX_MS, `${what} width`);
            assert.equal(board.cvWriteBefore(rises[i]).millivolts,
                noteMillivolts(STAGE_NOTES[expected.stage]), `${what} pitch`);
        });

        // The pattern comes round after twelve steps, and the clock output
        // has pulsed on every one of them, held or silent
        assertNear(gates[STAGE_GATES.length].riseMs - gates[0].riseMs,
            CYCLE_STEPS * STEP_MS, EDGE_JITTER_MAX_MS, 'cycle');
        assert.ok(clocks.length >= CYCLE_STEPS);
        for (let step = 1; step < CYCLE_STEPS; step++) {
            assertNear(clocks[step].riseMs - clocks[step - 1].riseMs, STEP_MS,
                EDGE_JITTER_MAX_MS, `clock period ${step}`);
            assertNear(clocks[step].widthMs, HALF_STEP_MS, EDGE_JITTER_MAX_MS,
                `clock width ${step}`);
        }
    });

    test(`${config}: any other setting of the mode switches plays the plain engine`, () => {
        // Switches above the low three choose nothing yet, and neither do
        // the numbers no engine has
        for (const modeSwitches of [MODE_PLAIN, 0xF8, 0x06, 0x07]) {
            const board = boardWith(config, modeSwitches);

            runUntilGates(board, 5);

            const gates = board.gatePulsesMs();
            const rises = board.gate.rises();

            // Steps 0 to 4 in turn, a gate of half a step each
            for (let step = 0; step < 5; step++) {
                const what = `mode ${modeSwitches} step ${step}`;

                assertNear(gates[step].riseMs - gates[0].riseMs, step * STEP_MS,
                    EDGE_JITTER_MAX_MS, `${what} rise`);
                assertNear(gates[step].widthMs, HALF_STEP_MS, EDGE_JITTER_MAX_MS,
                    `${what} width`);
                assert.equal(board.cvWriteBefore(rises[step]).millivolts,
                    noteMillivolts(STAGE_PANEL[step]), `${what} pitch`);
            }
        }
    });

    test(`${config}: the mode switch changed mid-pattern starts the other engine`, () => {
        const board = boardWith(config, MODE_PLAIN);

        runUntilGates(board, 3);
        board.setModeSwitches(MODE_STAGE);
        runUntilGates(board, 5);

        // Plain steps 0 to 2, then, when the next step falls due, the stage
        // engine from its stage 0, held for three steps
        const gates = board.gatePulsesMs();
        const rises = board.gate.rises();

        assertNear(gates[3].riseMs - gates[2].riseMs, STEP_MS, EDGE_JITTER_MAX_MS, 'next step');
        assertNear(gates[3].widthMs, (2 * STEP_MS) + HALF_STEP_MS, EDGE_JITTER_MAX_MS, 'held');
        assert.equal(board.cvWriteBefore(rises[3]).millivolts, noteMillivolts(STAGE_NOTES[0]));
        assert.equal(board.cvWriteBefore(rises[4]).millivolts, noteMillivolts(STAGE_NOTES[1]));
    });
}

test('debug: the stage engine logs a stage once for each of its steps', () => {
    const board = boardWith('debug', MODE_STAGE);
    const expected = [
        'Step 0 Note = 2', 'Step 0 Note = 2', 'Step 0 Note = 2',
        'Step 1 Note = 5', 'Step 1 Note = 5',
        'Step 2 Note = 8', 'Step 2 Rest',
        'Step 3 Rest',
        'Step 4 Note = 14', 'Step 5 Note = 1', 'Step 6 Note = 4', 'Step 7 Note = 7',
        'Step 0 Note = 2',
    ];

    board.runUntilLines(expected.length, RUN_LIMIT_MS);

    assert.deepEqual(board.serial.lines.map((line) => line.text), expected);
});

// The Klee engine. A stage's highest switch is its pattern switch and its
// low three its value. Stages 0 and 2 are loaded, over values 1 0 2 0 3 and
// then nothing: as the pair goes round, the sums are 3, 0, 5, 0, 3, nothing
// for nine steps, then 1 as the leading bit comes back to stage 0, and 0
const MODE_KLEE = 2;
const KLEE_LOAD = 0x08;
const KLEE_MERGE = 0x20;
const PATTERN = 8;
const KLEE_PANEL = [PATTERN | 1, 0, PATTERN | 2, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
const KLEE_GATES = [
    { step: 0, note: 3 }, { step: 2, note: 5 }, { step: 4, note: 3 },
    { step: 14, note: 1 }, { step: 16, note: 3 },
];
// One bit over the values 1 2 3 and then nothing: three notes in a row
const KLEE_RUN_PANEL = [PATTERN | 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];

// When each clock output pulse rose, finished or not: a gate that ends with
// its step's clock pulse is complete a moment before that pulse is
function clockRisesMs(board) {
    return board.clockOut.rises().map((cycle) => board.machine.cyclesToMs(cycle));
}

function kleeBoard(config, panel, modeSwitches) {
    const board = createBoard(config);

    board.setNotes(panel);
    board.setModeSwitches(modeSwitches);
    board.setTempoReading(READING_150_BPM);
    return board;
}

for (const config of CONFIGS) {
    test(`${config}: the Klee engine plays the sum under its active stages`, () => {
        const board = kleeBoard(config, KLEE_PANEL, MODE_KLEE);

        runUntilGates(board, KLEE_GATES.length);

        const gates = board.gatePulsesMs();
        const clocks = clockRisesMs(board);
        const rises = board.gate.rises();

        KLEE_GATES.forEach((expected, i) => {
            const what = `gate ${i}`;

            assertNear(gates[i].riseMs, clocks[expected.step], 0.001, `${what} rise`);
            assertNear(gates[i].widthMs, HALF_STEP_MS, EDGE_JITTER_MAX_MS, `${what} width`);
            assert.equal(board.cvWriteBefore(rises[i]).millivolts,
                noteMillivolts(expected.note), `${what} pitch`);
        });
    });

    test(`${config}: moving the Klee load switch loads the pattern again`, () => {
        const board = kleeBoard(config, KLEE_PANEL, MODE_KLEE);

        // Steps 0 and 2 have played and step 3, a rest, is next: the load
        // makes it the first step of the pattern instead
        runUntilGates(board, 2);
        board.setModeSwitches(MODE_KLEE | KLEE_LOAD);
        runUntilGates(board, 4);

        const gates = board.gatePulsesMs();
        const clocks = clockRisesMs(board);
        const rises = board.gate.rises();

        assertNear(gates[2].riseMs, clocks[3], 0.001, 'loaded step');
        assert.equal(board.cvWriteBefore(rises[2]).millivolts, noteMillivolts(3));
        assertNear(gates[3].riseMs, clocks[5], 0.001, 'the step after next');
        assert.equal(board.cvWriteBefore(rises[3]).millivolts, noteMillivolts(5));
    });

    test(`${config}: Klee merge runs consecutive notes into one gate`, () => {
        const board = kleeBoard(config, KLEE_RUN_PANEL, MODE_KLEE | KLEE_MERGE);

        runUntilGates(board, 2);

        const gates = board.gatePulsesMs();
        const clocks = board.clockOutPulsesMs();
        const clockRises = board.clockOut.rises();

        // One gate over the three notes, ended by the rest that follows,
        // and the next sixteen steps later; the pitch still moves with
        // each note
        assertNear(gates[0].riseMs, clocks[0].riseMs, 0.001, 'gate rise');
        assertNear(gates[0].widthMs, 3 * STEP_MS, EDGE_JITTER_MAX_MS, 'gate width');
        assertNear(gates[1].riseMs - gates[0].riseMs, 16 * STEP_MS, EDGE_JITTER_MAX_MS, 'cycle');
        [1, 2, 3].forEach((note, step) => {
            assert.equal(board.cvWriteBefore(clockRises[step]).millivolts,
                noteMillivolts(note), `pitch of step ${step}`);
        });
    });
}

test('debug: the Klee engine logs the register position and the sum', () => {
    const board = kleeBoard('debug', KLEE_PANEL, MODE_KLEE);
    const expected = [
        'Step 0 Note = 2', 'Step 1 Rest', 'Step 2 Note = 4', 'Step 3 Rest',
        'Step 4 Note = 2', 'Step 5 Rest',
    ];

    board.runUntilLines(expected.length, RUN_LIMIT_MS);

    assert.deepEqual(board.serial.lines.map((line) => line.text), expected);
});

// The engines that follow use a panel of sixteen notes with no rests. Where
// chance comes into it the firmware's random numbers start from the same
// seed at every power-on, so what they pick is known: the host tests work
// the same picks out from the generator's first bytes
const MODE_CHANCE = 3;
const MODE_CARTESIAN = 4;
const MODE_EUCLID = 5;
const CHANCE_SKIP_3_IN_4 = 0x18;
const CHANCE_SHUFFLE = 0x20;
const CARTESIAN_Y_EVERY_STEP = 0x60;
const EUCLID_5_BEATS = 4 << 3;
const KLEE_CHANCE_1_IN_8 = 0x40;
const NOTE_PANEL = [3, 6, 9, 12, 15, 2, 5, 8, 11, 14, 1, 4, 7, 10, 13, 2];
// One bit loaded at stage 0, and every stage's value 1: the note is the
// number of active stages
const KLEE_COUNT_PANEL = [PATTERN | 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1];

// Checks the first gates of a run: the clock pulse each rises with, and the
// note the DAC holds as it does
function assertGates(board, expected) {
    runUntilGates(board, expected.length);

    const gates = board.gatePulsesMs();
    const clocks = clockRisesMs(board);
    const rises = board.gate.rises();

    expected.forEach((gate, i) => {
        assertNear(gates[i].riseMs, clocks[gate.clock], 0.001, `gate ${i} rise`);
        assert.equal(board.cvWriteBefore(rises[i]).millivolts,
            noteMillivolts(gate.note), `gate ${i} pitch`);
    });
}

for (const config of CONFIGS) {
    test(`${config}: the chance engine shuffles the steps`, () => {
        const board = kleeBoard(config, NOTE_PANEL, MODE_CHANCE | CHANCE_SHUFFLE);

        assertGates(board, [7, 6, 0, 14].map(
            (step, clock) => ({ clock, note: NOTE_PANEL[step] })));
    });

    test(`${config}: the chance engine skips steps`, () => {
        const board = kleeBoard(config, NOTE_PANEL, MODE_CHANCE | CHANCE_SKIP_3_IN_4);

        // Of the first ten steps only these three are let through
        assertGates(board, [3, 4, 7].map((step) => ({ clock: step, note: NOTE_PANEL[step] })));
    });

    test(`${config}: the Cartesian engine crosses the grid by its two counters`, () => {
        const board = kleeBoard(config, NOTE_PANEL, MODE_CARTESIAN | CARTESIAN_Y_EVERY_STEP);

        assertGates(board, [0, 5, 10, 15, 0, 5].map(
            (step, clock) => ({ clock, note: NOTE_PANEL[step] })));
    });

    test(`${config}: the Euclidean engine sounds five beats spread over a bar`, () => {
        const board = kleeBoard(config, NOTE_PANEL, MODE_EUCLID | EUCLID_5_BEATS);

        assertGates(board, [0, 4, 7, 10, 13, 16, 20].map(
            (clock) => ({ clock, note: NOTE_PANEL[clock % 16] })));
    });

    test(`${config}: chance lets the Klee register drift`, () => {
        const board = kleeBoard(config, KLEE_COUNT_PANEL, MODE_KLEE | KLEE_CHANCE_1_IN_8);

        // Locked, one bit would go round playing 1 for ever. The first move
        // turns the bit coming round, and there are two from then on
        assertGates(board, [1, 2, 2, 2].map((note, clock) => ({ clock, note })));
    });
}

// The step lights: two 74HC595s that take what they are to show ahead of
// each panel read and show it when the load pulse ends, so a light follows
// its step by up to one 8 ms scan
const SCAN_MS = 8;
const LIGHT_LAG_MS = SCAN_MS + 1.5; // A tick to be given, a scan to be shown

function ledsAtMs(board, ms) {
    return board.ledsAt(board.machine.msToCycles(ms));
}

for (const config of CONFIGS) {
    test(`${config}: each step's light flashes for the first half of its step`, () => {
        const board = kleeBoard(config, NOTE_PANEL, MODE_PLAIN);

        board.runUntilClockOutPulses(18, RUN_LIMIT_MS);
        board.runForMs(2 * SCAN_MS); // For the last light to be seen going out

        const clocks = clockRisesMs(board);

        for (let step = 0; step < 18; step++) {
            assert.equal(ledsAtMs(board, clocks[step] + LIGHT_LAG_MS), 1 << (step % 16),
                `light of step ${step}`);
            assert.equal(ledsAtMs(board, clocks[step] + HALF_STEP_MS + LIGHT_LAG_MS), 0,
                `dark after step ${step}`);
        }

        // The lights' chips see everything sent on the bus, the DAC's frames
        // too, and must never show any of it: one light or none, changing
        // only with a panel read
        for (const latch of board.ledLatches) {
            assert.ok((latch.value & (latch.value - 1)) === 0, `shown: ${latch.value}`);
        }
        // One latch per read, and one more as the line was first set idle
        assert.equal(board.ledLatches.length, board.panelReads.length + 1);
    });

    test(`${config}: the lights show the Klee register`, () => {
        const board = kleeBoard(config, KLEE_PANEL, MODE_KLEE);

        board.runUntilClockOutPulses(3, RUN_LIMIT_MS);

        const clocks = clockRisesMs(board);

        // Stages 0 and 2 loaded, then one stage on with each step
        assert.equal(ledsAtMs(board, clocks[0] + LIGHT_LAG_MS), 0x0005);
        assert.equal(ledsAtMs(board, clocks[1] + LIGHT_LAG_MS), 0x000A);
        assert.equal(ledsAtMs(board, clocks[2] + LIGHT_LAG_MS), 0x0014);
    });
}
