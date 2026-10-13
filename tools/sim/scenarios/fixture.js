'use strict';
/**
 * What the scenario files share: the figures every one of them goes by and
 * the helpers they would each repeat. What only the MIDI scenarios share is
 * in midi_fixture.js. None of it comes from the firmware.
 */
const assert = require('node:assert/strict');

const READING_150_BPM = 480;     // Internal clock: exactly 100 ms a step
const STEP_MS = 100;
const HALF_STEP_MS = 50;         // The gate length and the clock's high time
const TICK_MS = 1;
// Outputs are set first thing in a tick, so their edges land on the tick to
// within the time the idle loop takes to notice it
const EDGE_JITTER_MAX_MS = 0.02;
const MILLIVOLTS_PER_OCTAVE = 1000;
const SEMITONES_PER_OCTAVE = 12;
const ERASED = 0xFF;             // An EEPROM byte never written

// The firmware plays the chromatic scale at 1 V to the octave: a note value
// is one semitone above the one before, and note value 1 is 0 V
function noteMillivolts(noteValue) {
    return Math.round(((noteValue - 1) * MILLIVOLTS_PER_OCTAVE) / SEMITONES_PER_OCTAVE);
}

function assertNear(actual, expected, tolerance, what) {
    assert.ok(Math.abs(actual - expected) <= tolerance,
        `${what}: ${actual} ms, expected ${expected} within ${tolerance}`);
}

function assertAllEqual(values, expected, what) {
    assert.ok(values.length > 0, `${what}: nothing to check`);
    assert.deepEqual(values, new Array(values.length).fill(expected), what);
}

/** The chip's time now, in milliseconds. */
function nowMs(board) {
    return board.machine.cyclesToMs(board.machine.cpu.cycles);
}

module.exports = {
    READING_150_BPM, STEP_MS, HALF_STEP_MS, TICK_MS, EDGE_JITTER_MAX_MS,
    MILLIVOLTS_PER_OCTAVE, SEMITONES_PER_OCTAVE, ERASED,
    noteMillivolts, assertNear, assertAllEqual, nowMs,
};
