'use strict';
// The mode switches and the engines they choose, on the emulated board, for
// both firmware images. The release image has no log, so steps are timed
// from the clock output and told apart by the voltage the DAC holds as each
// gate rises; the debug image's log lines are checked as well.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard } = require('../boards/nano_sequencer');

const CONFIGS = ['debug', 'release'];
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
        for (const modeSwitches of [MODE_PLAIN, 0xF8, 0x02, 0x07]) {
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
