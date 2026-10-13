'use strict';
// What the MIDI scenario files share: the figures they measure against and
// the helpers that set a board up and read what it sent.
const assert = require('node:assert/strict');
const { createBoard } = require('../boards/nano_sequencer');
const { parseMidi } = require('../boards/midi_messages');
const {
    READING_150_BPM, STEP_MS, HALF_STEP_MS, TICK_MS, MILLIVOLTS_PER_OCTAVE,
    SEMITONES_PER_OCTAVE,
} = require('./fixture');

const CPU_HZ = 16000000;
const MIDI_BAUD = 31250;
const BITS_PER_BYTE = 10;        // A start bit, eight data bits, a stop bit
const BYTE_CYCLES = (CPU_HZ / MIDI_BAUD) * BITS_PER_BYTE; // 5120: 320 us
const BYTE_MS = 0.32;
// A byte waiting in the queue goes on the loop pass that finds the last one
// gone, so two are this much further apart at most while nothing else is
// going on: 0.003 ms measured. Across a tick boundary it waits for what is
// left of the tick's work, which depends on where in it the last byte ended:
// 0.033 ms measured, and it moves with anything that changes a tick's length
const BYTE_GAP_SPARE_MS = 0.06;
const START = 0xFA;
const CLOCK = 0xF8;
const NOTE_ON_CHANNEL_1 = 0x90;
const VELOCITY = 100;
const LOWEST_NOTE = 24;          // C1: note value 1 on the panel, 0 V
const CLOCKS_PER_STEP = 6;       // A step is a sixteenth note
const STEPS_PER_QUARTER = 4;
const CLOCKS_PER_QUARTER = 24;
const UBRR_31250_DOUBLE_SPEED = 63;
const DOUBLE_SPEED_SAMPLES = 8;  // Clock cycles of the divider to a bit
const READING_ONE_TO_ONE = 500;  // External clock: a step to an edge
const READING_MULTIPLY_4 = 1023; // Four steps to an edge
const SETTLE_MS = 5;             // For what a tick queued to have been sent
const CLOCK_MS = STEP_MS / CLOCKS_PER_STEP; // 16.667 ms at 150 BPM
// Pulses are counted on the 1 ms tick, so a clock due every 16.667 ms goes
// on the 16th or 17th tick after the last, and can wait behind the bytes of
// a note or for a tick that also reads the panel: 16.00 to 17.24 measured
const CLOCK_JITTER_MAX_MS = 0.7;
const RUN_LIMIT_MS = 10000;
const DEBOUNCE_MS = 20;
// How long after its gate edge a note's message begins. It is queued in the
// same tick, after the pins are written, and waits for the bytes ahead of
// it: 0.40 ms behind a clock and 0.73 ms behind a Start and a clock,
// measured. Up to 0.25 ms more on a tick that also reads the panel
const NOTE_AFTER_GATE_MIN_MS = 0.3;
const NOTE_AFTER_GATE_MAX_MS = 1.0;
// A Start, a Stop or a step's first clock is at the head of the queue: it
// begins as soon as the rest of the tick's work is done. 0.08 ms measured,
// and 0.40 ms when a Note Off from the tick before is still going, before
// the LFO was read on every tick; that read is about 0.1 ms more of the
// tick's work, and the first step's clock, behind a Start, is at 0.58 ms
const HEAD_AFTER_PIN_MAX_MS = 0.7;
// Sixteen notes, no rests, and all but the last different
const PANEL = [3, 6, 9, 12, 15, 2, 5, 8, 11, 14, 1, 4, 7, 10, 13, 2];
// The release scenarios' panel: step 5 is a rest
const REST_PANEL = [1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2, 5, 8, 11, 14];
const MODE_STAGE = 1;
const MODE_KLEE = 2;
const KLEE_MERGE = 0x20;
const KLEE_PATTERN = 8;

// The note number a note value on the panel should be sent as: the firmware
// plays the chromatic scale, note value 1 the lowest
function panelNote(noteValue) {
    return LOWEST_NOTE + (noteValue - 1);
}

// The note number of a voltage at 1 V to the octave, 0 V the lowest
function voltageNote(millivolts) {
    return LOWEST_NOTE
        + Math.round((millivolts * SEMITONES_PER_OCTAVE) / MILLIVOLTS_PER_OCTAVE);
}

function midiBoard(panel, reading, external = false, modeSwitches = 0) {
    const board = createBoard('midi');

    board.setNotes(panel);
    board.setModeSwitches(modeSwitches);
    board.setTempoReading(reading);
    if (external) {
        board.startOnExternalClock();
    }
    return board;
}

function ms(board, cycles) {
    return board.machine.cyclesToMs(cycles);
}

function ofType(board, type) {
    return board.midi.messages.filter((message) => message.type === type);
}

// Everything sent but the clocks, as 'start', 'stop', 'on 26' or 'off 26'
function withoutClocks(messages) {
    return messages.filter((message) => message.type !== 'clock').map((message) => {
        if (message.type === 'noteOn') {
            return `on ${message.note}`;
        }
        return (message.type === 'noteOff') ? `off ${message.note}` : message.type;
    });
}

function assertBetween(actual, min, max, what) {
    assert.ok((actual >= min) && (actual <= max),
        `${what}: ${actual}, expected ${min} to ${max}`);
}

// Checks that every byte sent was read as one of the five messages, each
// note on channel 1 with the one velocity, and that the board's messages are
// what the capture reads as from its first byte
function assertWellFormed(board) {
    assert.deepEqual(parseMidi(board.midi.bytes), board.midi.messages);
    for (const message of board.midi.messages) {
        assert.notEqual(message.type, 'unknown',
            `byte ${message.value} at ${ms(board, message.cycle)} ms`);
        if (message.type === 'noteOn') {
            assert.equal(message.channel, 1);
            assert.equal(message.velocity, VELOCITY);
        } else if (message.type === 'noteOff') {
            assert.equal(message.channel, 1);
            assert.equal(message.velocity, 0);
        } else {
            // Start, Stop and the clock are a byte each
        }
    }
}

// Checks that no byte was handed over before the one ahead of it had gone
function assertNoByteEarly(board) {
    const { bytes } = board.midi;

    for (let i = 1; i < bytes.length; i++) {
        assert.ok((bytes[i].cycle - bytes[i - 1].cycle) >= BYTE_CYCLES,
            `byte ${i} began ${bytes[i].cycle - bytes[i - 1].cycle} cycles after the last`);
    }
}

// Plays the note messages back as a receiver would hear them. Checks that
// every Note Off is for a note that is sounding and that no more than two
// ever sound together, and returns the notes still sounding at the end
function soundingAtEnd(board) {
    const sounding = [];

    for (const message of board.midi.messages) {
        if (message.type === 'noteOn') {
            sounding.push(message.note);
            assert.ok(sounding.length <= 2, `${sounding.length} notes sounding`);
        } else if (message.type === 'noteOff') {
            const at = sounding.indexOf(message.note);

            assert.notEqual(at, -1, `Note Off ${message.note} with no such note sounding`);
            sounding.splice(at, 1);
        } else {
            // Not a note
        }
    }
    return sounding;
}

// How many clocks began from one cycle up to, not including, another
function clocksBetween(board, from, to) {
    return ofType(board, 'clock')
        .filter((clock) => (clock.cycle >= from) && (clock.cycle < to)).length;
}

// Checks there are six clocks from each step's clock output pulse to the
// next, the first of them the downbeat that follows the pulse
function assertSixClocksAStep(board, firstStep = 0) {
    const rises = board.clockOut.rises();

    for (let step = firstStep; step < rises.length - 1; step++) {
        assert.equal(clocksBetween(board, rises[step], rises[step + 1]), CLOCKS_PER_STEP,
            `clocks in step ${step}`);
    }
}

function clockPeriodsMs(board, clocks) {
    const periods = [];

    for (let i = 1; i < clocks.length; i++) {
        periods.push(ms(board, clocks[i].cycle - clocks[i - 1].cycle));
    }
    return periods;
}

module.exports = {
    MIDI_BAUD, BITS_PER_BYTE, BYTE_CYCLES, BYTE_MS, BYTE_GAP_SPARE_MS,
    START, CLOCK, NOTE_ON_CHANNEL_1, VELOCITY,
    CLOCKS_PER_STEP, STEPS_PER_QUARTER, CLOCKS_PER_QUARTER,
    MILLIVOLTS_PER_OCTAVE, SEMITONES_PER_OCTAVE, UBRR_31250_DOUBLE_SPEED,
    DOUBLE_SPEED_SAMPLES, READING_150_BPM, READING_ONE_TO_ONE,
    READING_MULTIPLY_4, STEP_MS, HALF_STEP_MS, SETTLE_MS, CLOCK_MS,
    CLOCK_JITTER_MAX_MS, TICK_MS, RUN_LIMIT_MS, DEBOUNCE_MS,
    NOTE_AFTER_GATE_MIN_MS, NOTE_AFTER_GATE_MAX_MS, HEAD_AFTER_PIN_MAX_MS,
    PANEL, REST_PANEL, MODE_STAGE, MODE_KLEE, KLEE_MERGE, KLEE_PATTERN,
    panelNote, voltageNote, midiBoard, ms, ofType, withoutClocks,
    assertBetween, assertWellFormed, assertNoByteEarly, soundingAtEnd,
    clocksBetween, assertSixClocksAStep, clockPeriodsMs,
};
