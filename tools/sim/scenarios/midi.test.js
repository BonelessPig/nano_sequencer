'use strict';
// MIDI out on the emulated board: what the midi firmware image sends from
// the USART, timed against the gate and the clock output, and that the other
// two images send none. The bytes are read into messages by
// boards/midi_messages.js, which was worked out from the MIDI specification
// and not from the firmware.
//
// A byte's cycle is the one in which the firmware handed it to the USART,
// which is when its start bit begins. The emulated USART holds one byte and
// takes the next only when that one has gone, where the real one has a
// buffer behind its shift register, so there a byte behind another leaves
// the pin with no wait for the loop to come round, a few microseconds
// earlier than here. Nothing electrical is modelled: these show which bytes
// are sent and when, not that a MIDI input would read them.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard, STEP_COUNT } = require('../boards/nano_sequencer');
const { parseMidi, midiNoteName } = require('../boards/midi_messages');

const CPU_HZ = 16000000;
const MIDI_BAUD = 31250;
const BITS_PER_BYTE = 10;        // A start bit, eight data bits, a stop bit
const BYTE_CYCLES = (CPU_HZ / MIDI_BAUD) * BITS_PER_BYTE; // 5120: 320 us
const BYTE_MS = 0.32;
// A byte waiting in the queue goes on the loop pass that finds the last one
// gone, so two are this much further apart at most while nothing else is
// going on: 0.003 ms measured, and 0.024 ms across a tick boundary
const BYTE_GAP_SPARE_MS = 0.03;
const START = 0xFA;
const STOP = 0xFC;
const CLOCK = 0xF8;
const NOTE_ON_CHANNEL_1 = 0x90;
const NOTE_OFF_CHANNEL_1 = 0x80;
const VELOCITY = 100;
const LOWEST_NOTE = 24;          // C1: note value 1 on the panel, 0 V
const CLOCKS_PER_STEP = 6;       // A step is a sixteenth note
const STEPS_PER_QUARTER = 4;
const CLOCKS_PER_QUARTER = 24;
const MILLIVOLTS_PER_OCTAVE = 1000;
const SEMITONES_PER_OCTAVE = 12;
const UBRR_31250_DOUBLE_SPEED = 63;
const DOUBLE_SPEED_SAMPLES = 8;  // Clock cycles of the divider to a bit
const READING_150_BPM = 480;     // Internal clock: exactly 100 ms a step
const READING_ONE_TO_ONE = 500;  // External clock: a step to an edge
const READING_MULTIPLY_4 = 1023; // Four steps to an edge
const STEP_MS = 100;
const HALF_STEP_MS = 50;         // The gate length and the clock's high time
const SETTLE_MS = 5;             // For what a tick queued to have been sent
const CLOCK_MS = STEP_MS / CLOCKS_PER_STEP; // 16.667 ms at 150 BPM
// Pulses are counted on the 1 ms tick, so a clock due every 16.667 ms goes
// on the 16th or 17th tick after the last, and can wait behind the bytes of
// a note or for a tick that also reads the panel: 16.00 to 17.24 measured
const CLOCK_JITTER_MAX_MS = 0.7;
const TICK_MS = 1;
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
// and 0.40 ms when a Note Off from the tick before is still going
const HEAD_AFTER_PIN_MAX_MS = 0.5;
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

function midiBoard(panel, reading, syncOn = false, modeSwitches = 0) {
    const board = createBoard('midi');

    board.setNotes(panel);
    board.setModeSwitches(modeSwitches);
    board.setTempoReading(reading);
    board.setSyncSwitch(syncOn);
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

test('midi_messages reads a capture into messages, by the specification', () => {
    const capture = [
        0xFA,                   // Start
        0xF8,                   // Clock
        0x90, 0x3C, 0xF8, 0x64, // Note On, C4, with a clock between its bytes
        0x81, 0x45, 0x00,       // Note Off on channel 2, A4
        0x3C, 0x00,             // Running status, which is not accepted
        0xFC,                   // Stop
        0xB0,                   // A status that is none of the five
        0x90, 0x3C,             // A Note On cut short...
        0x80, 0x3C, 0x00,       // ...by a Note Off
        0xFE,                   // A real-time byte that is none of the three
        0x90, 0x18,             // Unfinished at the end: left out
    ].map((value, i) => ({ value, cycle: i * BYTE_CYCLES }));
    const at = (i) => i * BYTE_CYCLES;

    assert.deepEqual(parseMidi(capture), [
        { type: 'start', cycle: at(0) },
        { type: 'clock', cycle: at(1) },
        { type: 'clock', cycle: at(4) },
        { type: 'noteOn', channel: 1, note: 60, velocity: 100, cycle: at(2) },
        { type: 'noteOff', channel: 2, note: 69, velocity: 0, cycle: at(6) },
        { type: 'unknown', value: 0x3C, cycle: at(9) },
        { type: 'unknown', value: 0x00, cycle: at(10) },
        { type: 'stop', cycle: at(11) },
        { type: 'unknown', value: 0xB0, cycle: at(12) },
        { type: 'unknown', value: 0x90, cycle: at(13) },
        { type: 'noteOff', channel: 1, note: 60, velocity: 0, cycle: at(15) },
        { type: 'unknown', value: 0xFE, cycle: at(18) },
    ]);
    assert.deepEqual([24, 25, 60, 69].map(midiNoteName), ['C1', 'C#1', 'C4', 'A4']);
});

test('midi: sends at 31250 baud, 8N1, a byte every 320 us when they queue', () => {
    const board = midiBoard(PANEL, READING_150_BPM);
    const { usart } = board.machine;

    // Start, a clock and a Note On: five bytes one behind the other
    board.midi.bytes.length = 0;
    board.machine.runUntil(() => board.midi.bytes.length >= 5, RUN_LIMIT_MS);

    // 16 MHz over 8 samples a bit and a divisor of 64 is 31250 exactly
    assert.equal(usart.UBRR, UBRR_31250_DOUBLE_SPEED);
    assert.equal(usart.multiplier, DOUBLE_SPEED_SAMPLES);
    assert.equal(usart.baudRate, MIDI_BAUD);
    assert.equal(usart.bitsPerChar, 8);
    assert.equal(usart.stopBits, 1);
    assert.equal(usart.parityEnabled, false);
    assert.equal(usart.txEnable, true);
    assert.equal(usart.rxEnable, true);
    assert.equal(usart.cyclesPerChar, BYTE_CYCLES);

    for (let i = 1; i < 5; i++) {
        const gapMs = ms(board, board.midi.bytes[i].cycle - board.midi.bytes[i - 1].cycle);

        assertBetween(gapMs, BYTE_MS, BYTE_MS + BYTE_GAP_SPARE_MS, `ms from byte ${i - 1} to ${i}`);
    }
});

test('midi: at power-on sends Start, a clock, then the first step\'s Note On', () => {
    const board = midiBoard(PANEL, READING_150_BPM);

    board.runUntilMidiMessages(3, RUN_LIMIT_MS);

    const note = panelNote(PANEL[0]);
    const gateRise = board.gate.rises()[0];
    const [start, clock, noteOn] = board.midi.messages;

    assert.deepEqual(board.midi.bytes.map((byte) => byte.value),
        [START, CLOCK, NOTE_ON_CHANNEL_1, note, VELOCITY]);
    assert.deepEqual(withoutClocks(board.midi.messages), ['start', `on ${note}`]);
    assert.equal(clock.type, 'clock');
    assert.equal(note, voltageNote(board.cvWriteBefore(gateRise).millivolts));
    assertWellFormed(board);

    // The gate rises on the second tick and the bytes are queued just
    // after it: the Start at once, the note behind it and the clock
    assertBetween(ms(board, start.cycle - gateRise), 0, HEAD_AFTER_PIN_MAX_MS,
        'ms from the gate to Start');
    assertBetween(ms(board, noteOn.cycle - gateRise),
        2 * BYTE_MS, NOTE_AFTER_GATE_MAX_MS, 'ms from the gate to Note On');
    assertBetween(ms(board, start.cycle), 2 * TICK_MS, 3 * TICK_MS, 'ms from reset to Start');
});

test('midi: six clocks a step and 24 a quarter note, steady, over 64 steps', () => {
    const board = midiBoard(PANEL, READING_150_BPM);
    const steps = 4 * STEP_COUNT;

    // The first step begins on the second tick. The run ends just over
    // half way into the step after the last one counted, with that step's
    // downbeat and the three clocks after it sent
    board.runForMs((2 * TICK_MS) + (steps * STEP_MS) + HALF_STEP_MS + SETTLE_MS);

    const rises = board.clockOut.rises();
    const clocks = ofType(board, 'clock');

    assert.equal(rises.length, steps + 1);
    assert.equal(clocks.length, (steps * CLOCKS_PER_STEP) + (CLOCKS_PER_STEP / 2) + 1);
    assertSixClocksAStep(board);
    for (let step = 0; step < steps; step += STEPS_PER_QUARTER) {
        assert.equal(clocksBetween(board, rises[step], rises[step + STEPS_PER_QUARTER]),
            CLOCKS_PER_QUARTER, `clocks in the quarter note from step ${step}`);
    }

    // Each step's first clock is its downbeat, just behind the pulse
    rises.forEach((rise, step) => {
        const downbeat = clocks[step * CLOCKS_PER_STEP];

        assertBetween(ms(board, downbeat.cycle - rise), 0, HEAD_AFTER_PIN_MAX_MS,
            `ms from step ${step}'s pulse to its clock`);
    });

    for (const periodMs of clockPeriodsMs(board, clocks)) {
        assertBetween(periodMs, CLOCK_MS - CLOCK_JITTER_MAX_MS, CLOCK_MS + CLOCK_JITTER_MAX_MS,
            'clock period in ms');
    }
    // And no drift: 384 clock periods take 64 steps, to within one clock's
    // own wait in the queue
    const downbeats = [clocks[0], clocks[steps * CLOCKS_PER_STEP]];

    assertBetween(ms(board, downbeats[1].cycle - downbeats[0].cycle),
        (steps * STEP_MS) - HEAD_AFTER_PIN_MAX_MS, (steps * STEP_MS) + HEAD_AFTER_PIN_MAX_MS,
        'ms for 64 steps of clocks');
    assertWellFormed(board);
    assertNoByteEarly(board);
});

test('midi: a Note On for every gate, a Note Off as it falls, nothing for a rest', () => {
    const board = midiBoard(REST_PANEL, READING_150_BPM);
    const steps = 2 * STEP_COUNT;
    const noteSteps = [];

    for (let step = 0; step <= steps; step++) {
        if (REST_PANEL[step % STEP_COUNT] !== 0) {
            noteSteps.push(step);
        }
    }
    // Two passes and the first step of a third, its gate fallen and its
    // Note Off given time to go
    board.runForMs((2 * TICK_MS) + (steps * STEP_MS) + HALF_STEP_MS + SETTLE_MS);

    const rises = board.gate.rises();
    const falls = board.gate.edges.filter((edge) => !edge.high).map((edge) => edge.cycle);
    const clockRises = board.clockOut.rises();
    const noteOns = ofType(board, 'noteOn');
    const noteOffs = ofType(board, 'noteOff');

    assert.equal(rises.length, noteSteps.length);
    assert.equal(falls.length, noteSteps.length);
    assert.equal(noteOns.length, rises.length);
    assert.equal(noteOffs.length, falls.length);
    noteSteps.forEach((step, i) => {
        const note = panelNote(REST_PANEL[step % STEP_COUNT]);

        // The note of the step, which is the note of the voltage the DAC
        // held as the gate rose
        assert.equal(noteOns[i].note, note, `step ${step}'s Note On`);
        assert.equal(noteOns[i].note, voltageNote(board.cvWriteBefore(rises[i]).millivolts));
        assert.equal(noteOffs[i].note, note, `step ${step}'s Note Off`);
        assert.ok(rises[i] > clockRises[step] - board.machine.msToCycles(TICK_MS / 2),
            `gate ${i} is step ${step}'s`);
        assertBetween(ms(board, noteOns[i].cycle - rises[i]),
            NOTE_AFTER_GATE_MIN_MS, NOTE_AFTER_GATE_MAX_MS, `ms from gate ${i} to its Note On`);
        assertBetween(ms(board, noteOffs[i].cycle - falls[i]),
            NOTE_AFTER_GATE_MIN_MS, NOTE_AFTER_GATE_MAX_MS, `ms from gate ${i} to its Note Off`);
    });

    // A rest sends its six clocks and nothing else
    for (const step of [5, STEP_COUNT + 5]) {
        const inStep = board.midi.messages.filter((message) => (message.cycle >= clockRises[step])
            && (message.cycle < clockRises[step + 1]));

        assert.deepEqual(inStep.map((message) => message.type), new Array(6).fill('clock'));
    }
    assert.deepEqual(soundingAtEnd(board), []);
    assertWellFormed(board);
    assertNoByteEarly(board);
});

test('midi: the run button sends Stop and Start, and the clock carries on between', () => {
    const board = midiBoard(PANEL, READING_150_BPM);

    board.runUntilClockOutPulses(2, RUN_LIMIT_MS);
    board.runForMs(55);           // Just into step 2, with its gate open
    board.pressRunButton(60, 5);

    // The note is cut off as the press counts: Stop, then its Note Off
    const cut = board.gate.edges[board.gate.edges.length - 1];
    const stop = ofType(board, 'stop')[0];
    const lastOff = ofType(board, 'noteOff').pop();

    assert.equal(cut.high, false);
    assert.deepEqual(withoutClocks(board.midi.messages), [
        'start', `on ${panelNote(PANEL[0])}`, `off ${panelNote(PANEL[0])}`,
        `on ${panelNote(PANEL[1])}`, `off ${panelNote(PANEL[1])}`,
        `on ${panelNote(PANEL[2])}`, 'stop', `off ${panelNote(PANEL[2])}`,
    ]);
    assertBetween(ms(board, stop.cycle - cut.cycle), 0, HEAD_AFTER_PIN_MAX_MS,
        'ms from the gate cut to Stop');
    assertBetween(ms(board, lastOff.cycle - cut.cycle), BYTE_MS, NOTE_AFTER_GATE_MAX_MS,
        'ms from the gate cut to Note Off');

    // Stopped, with the sync switch off: no step and no note, and the
    // clock goes on at the tempo as if nothing had happened
    const stepsBefore = board.clockOut.rises().length;
    const messagesBefore = board.midi.messages.length;

    board.runForMs(450);
    assert.equal(board.clockOut.rises().length, stepsBefore);

    const stoppedClocks = board.midi.messages.slice(messagesBefore);
    const allClocks = ofType(board, 'clock');

    assert.ok(stoppedClocks.every((message) => message.type === 'clock'));
    assertBetween(stoppedClocks.length, 26, 28, 'clocks in 450 ms stopped');
    for (const periodMs of clockPeriodsMs(board, allClocks)) {
        assertBetween(periodMs, CLOCK_MS - CLOCK_JITTER_MAX_MS, CLOCK_MS + CLOCK_JITTER_MAX_MS,
            'clock period in ms, through the stop');
    }

    // The next press: Start, a clock at once, and step 0's note
    const startedAtMs = ms(board, board.machine.cpu.cycles);
    const messagesStopped = board.midi.messages.length;

    board.pressRunButton(60, 5);
    board.runForMs(200);

    const restartRise = board.clockOut.rises()[stepsBefore];
    const sinceStop = board.midi.messages.slice(messagesStopped);
    const at = sinceStop.findIndex((message) => message.type === 'start');
    const [start, downbeat, noteOn] = sinceStop.slice(at);

    assert.ok(sinceStop.slice(0, at).every((message) => message.type === 'clock'));
    assert.equal(downbeat.type, 'clock');
    assert.equal(noteOn.type, 'noteOn');
    assert.equal(noteOn.note, panelNote(PANEL[0]));
    assert.equal(ofType(board, 'start').length, 2);
    assertBetween(ms(board, start.cycle) - startedAtMs, DEBOUNCE_MS, DEBOUNCE_MS + 8,
        'ms from the press to Start');
    assertBetween(ms(board, start.cycle - restartRise), 0, HEAD_AFTER_PIN_MAX_MS,
        'ms from the first step\'s pulse to Start');
    assertBetween(ms(board, noteOn.cycle - restartRise), 2 * BYTE_MS, NOTE_AFTER_GATE_MAX_MS,
        'ms from the first step\'s pulse to its Note On');
    // The clocks count from the Start again: six to each step after it
    assertSixClocksAStep(board, stepsBefore);
    assert.deepEqual(withoutClocks(sinceStop.slice(at)).slice(0, 4), [
        'start', `on ${panelNote(PANEL[0])}`, `off ${panelNote(PANEL[0])}`,
        `on ${panelNote(PANEL[1])}`,
    ]);
    assertWellFormed(board);
});

test('midi: a stage held over three steps is one Note On and one Note Off', () => {
    // Stage 0 holds for three steps (its control is step 8: mode 2, hold,
    // in the high two switches and the length less one in the low two);
    // the other stages are a step each
    const panel = [3, 6, 9, 12, 15, 2, 5, 8, (2 << 2) | 2, 0, 0, 0, 0, 0, 0, 0];
    const board = midiBoard(panel, READING_150_BPM, false, MODE_STAGE);

    board.runUntilClockOutPulses(5, RUN_LIMIT_MS);

    const gates = board.gatePulsesMs();
    const clockRises = board.clockOut.rises();
    const held = panelNote(panel[0]);

    // One gate over steps 0 to 2, two and a half steps long, then stage 1
    assertBetween(gates[0].widthMs, 249.9, 250.1, 'held gate in ms');
    assert.deepEqual(withoutClocks(board.midi.messages), [
        'start', `on ${held}`, `off ${held}`,
        `on ${panelNote(panel[1])}`, `off ${panelNote(panel[1])}`,
        `on ${panelNote(panel[2])}`,
    ]);
    assertBetween(ms(board, ofType(board, 'noteOff')[0].cycle - ofType(board, 'noteOn')[0].cycle),
        249, 251, 'ms the held note sounded');
    // The steps it is held through send their clocks and nothing else
    assert.equal(clocksBetween(board, clockRises[0], clockRises[3]), 3 * CLOCKS_PER_STEP);
    assertSixClocksAStep(board);
    assertWellFormed(board);
});

test('midi: a tie to another pitch sends the new Note On, then the old Note Off', () => {
    // The Klee engine with merge on: one bit over the values 1, 2 and 3
    // and then nothing, which is three notes under one gate
    const panel = [KLEE_PATTERN | 1, 2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0];
    const board = midiBoard(panel, READING_150_BPM, false, MODE_KLEE | KLEE_MERGE);

    board.runUntilClockOutPulses(4, RUN_LIMIT_MS);

    const [first, second, third] = [1, 2, 3].map(panelNote);
    const notes = board.midi.messages.filter((message) => message.type.startsWith('note'));

    assert.equal(board.gate.rises().length, 1);
    assertBetween(board.gatePulsesMs()[0].widthMs, 299.9, 300.1, 'tied gate in ms');
    assert.deepEqual(withoutClocks(board.midi.messages), [
        'start', `on ${first}`, `on ${second}`, `off ${first}`,
        `on ${third}`, `off ${second}`, `off ${third}`,
    ]);
    // The old note's Note Off is right behind the new Note On: three bytes
    for (const i of [1, 3]) {
        assertBetween(ms(board, notes[i + 1].cycle - notes[i].cycle),
            3 * BYTE_MS, 3 * (BYTE_MS + BYTE_GAP_SPARE_MS), `ms from tie ${i}'s On to its Off`);
    }
    assert.deepEqual(soundingAtEnd(board), []);
    assertWellFormed(board);
});

test('midi: on the external clock Start goes at once and the clocks wait for an edge', () => {
    const board = midiBoard(PANEL, READING_ONE_TO_ONE, true);

    // No clock plugged in: Start, and then nothing however long
    board.runForMs(400);
    assert.deepEqual(board.midi.messages.map((message) => message.type), ['start']);
    assertBetween(ms(board, board.midi.messages[0].cycle), 2 * TICK_MS, 3 * TICK_MS,
        'ms from reset to Start');

    const edges = 6;

    board.startClockIn(80);
    board.runForMs((edges * 80) - 10);
    board.stopClockIn();
    board.runForMs(200);

    const clocks = ofType(board, 'clock');
    const rises = board.clockOut.rises();
    const firstEdge = board.clockIn.rises[0];

    // The first clock is the first step's: taken on the tick after the
    // edge, sent on the one after, behind the pins
    assert.equal(board.clockIn.rises.length, edges);
    assert.equal(rises.length, edges);
    assertBetween(ms(board, clocks[0].cycle - firstEdge), TICK_MS,
        (2 * TICK_MS) + HEAD_AFTER_PIN_MAX_MS, 'ms from the first edge to the first clock');
    assert.ok(clocks[0].cycle > rises[0]);
    // Six clocks to an edge at one step to an edge, the last six after the
    // last edge; and every step its note
    assert.equal(clocks.length, edges * CLOCKS_PER_STEP);
    assertSixClocksAStep(board);
    assert.deepEqual(ofType(board, 'noteOn').map((message) => message.note),
        PANEL.slice(0, edges).map(panelNote));
    assert.deepEqual(soundingAtEnd(board), []);
    assertWellFormed(board);
});

test('midi: an external clock multiplied by four gets 24 clocks to an edge', () => {
    const board = midiBoard(PANEL, READING_MULTIPLY_4, true);
    const edges = 5;

    board.runForMs(10);
    board.startClockIn(100);
    board.runForMs((edges * 100) - 10);
    board.stopClockIn();
    board.runForMs(200);

    // One step for the first edge, whose spacing is not yet known, and
    // four to an edge from the second; six clocks to every one of them
    const rises = board.clockOut.rises();
    const steps = 1 + ((edges - 1) * 4);

    assert.equal(board.clockIn.rises.length, edges);
    assert.equal(rises.length, steps);
    assert.equal(ofType(board, 'clock').length, steps * CLOCKS_PER_STEP);
    assertSixClocksAStep(board);
    for (let step = 1; step + 4 < rises.length; step += 4) {
        assert.equal(clocksBetween(board, rises[step], rises[step + 4]), CLOCKS_PER_QUARTER,
            `clocks from the edge that began step ${step} to the next`);
    }
    assert.equal(ofType(board, 'noteOn').length, steps);
    assert.deepEqual(soundingAtEnd(board), []);
    assertWellFormed(board);
});

test('midi: stopped with the sync switch on sends no clocks, whatever comes in', () => {
    const board = midiBoard(PANEL, READING_ONE_TO_ONE, true);

    board.runForMs(10);
    board.startClockIn(80);
    board.runForMs(220);          // Edges at 10, 90 and 170 ms
    board.pressRunButton(60);
    board.runForMs(10);           // For whatever the stop sent to have gone

    const stopAt = board.midi.messages.findIndex((message) => message.type === 'stop');
    const sentBefore = board.midi.messages.length;

    assert.notEqual(stopAt, -1);
    assert.ok(board.midi.messages.slice(stopAt + 1).every((message) => message.type === 'noteOff'));
    assert.deepEqual(soundingAtEnd(board), []);

    // The clock source is still running; none of it is passed on
    board.runForMs(500);
    assert.equal(board.midi.messages.length, sentBefore);

    // Started again: Start as the press counts, the first clock at the
    // next edge, and step 0's note behind it
    board.pressRunButton(60);
    board.runForMs(100);

    const since = board.midi.messages.slice(sentBefore);
    const start = since[0];
    const nextEdge = board.clockIn.rises.find((edge) => edge > start.cycle);

    assert.deepEqual(since.slice(0, 3).map((message) => message.type),
        ['start', 'clock', 'noteOn']);
    assert.equal(since[2].note, panelNote(PANEL[0]));
    assertBetween(ms(board, since[1].cycle - nextEdge), TICK_MS,
        (2 * TICK_MS) + HEAD_AFTER_PIN_MAX_MS, 'ms from the next edge to the first clock');
    assertWellFormed(board);
});

test('midi: a step every 5 ms loses nothing: every clock, every note', () => {
    const board = midiBoard(PANEL, READING_MULTIPLY_4, true);

    // An edge every 20 ms, four steps to each: twelve bytes every 5 ms,
    // which is 3.84 ms of the line's time
    board.runForMs(10);
    board.startClockIn(20);
    board.runForMs(400);
    board.stopClockIn();
    board.runForMs(200);

    const edges = board.clockIn.rises.length;
    const steps = 1 + ((edges - 1) * 4);

    assert.equal(edges, 20);
    assert.equal(board.clockOut.rises().length, steps);
    assert.equal(board.gate.rises().length, steps);
    assert.equal(ofType(board, 'clock').length, steps * CLOCKS_PER_STEP);
    assert.equal(ofType(board, 'clock').length,
        CLOCKS_PER_STEP + ((edges - 1) * CLOCKS_PER_QUARTER));
    assert.equal(ofType(board, 'noteOn').length, steps);
    assert.equal(ofType(board, 'noteOff').length, steps);
    assert.deepEqual(ofType(board, 'noteOn').map((message) => message.note),
        board.gate.rises().map((rise) => voltageNote(board.cvWriteBefore(rise).millivolts)));
    assert.deepEqual(soundingAtEnd(board), []);
    assertWellFormed(board);
    assertNoByteEarly(board);
});

test('midi: with more to send than the line can carry, clocks are kept and notes go', () => {
    const board = midiBoard(PANEL, READING_MULTIPLY_4, true);

    // An edge every 12 ms, four steps to each: twelve bytes every 3 ms,
    // which would take 3.84 ms. Far past any tempo; it is here to fill the
    // queue. The clocks are owed and all sent in the end, a note that could
    // not be sent before its gate fell is given up whole, and no byte is
    // dropped from the middle of a message
    board.runForMs(10);
    board.startClockIn(12);
    board.runForMs(400);
    board.stopClockIn();
    board.runForMs(200);

    const edges = board.clockIn.rises.length;
    const steps = 1 + ((edges - 1) * 4);
    const noteOns = ofType(board, 'noteOn').length;

    assert.equal(board.clockOut.rises().length, steps);
    assert.equal(ofType(board, 'clock').length, steps * CLOCKS_PER_STEP);
    assert.ok((noteOns > 0) && (noteOns < steps), `${noteOns} notes for ${steps} steps`);
    assert.equal(ofType(board, 'noteOff').length, noteOns);
    assert.deepEqual(soundingAtEnd(board), []);
    assertWellFormed(board);
    assertNoByteEarly(board);
});

test('midi: calibration mode sends nothing', () => {
    const board = createBoard('midi');

    board.setCalibrationPanel(2, true);
    board.setTempoReading(READING_150_BPM);
    board.runForMs(500);

    // In the mode: the gate up for the meter and nothing sequenced
    assert.equal(board.gate.rises().length, 1);
    assert.equal(board.clockOut.edges.length, 0);
    assert.equal(board.serial.bytes, 0);
    assert.deepEqual(board.midi.bytes, []);
});

test('midi: the gate, the clock output and the pitch are the release image\'s', () => {
    const boards = ['release', 'midi'].map((config) => {
        const board = createBoard(config);

        board.setNotes(REST_PANEL);
        board.setTempoReading(READING_150_BPM);
        board.runUntilClockOutPulses(STEP_COUNT + 1, RUN_LIMIT_MS);
        return board;
    });
    const [release, midi] = boards;
    const pitches = (board) => board.gate.rises()
        .map((rise) => board.cvWriteBefore(rise).millivolts);

    // The outputs are written first in a tick, ahead of the MIDI bytes
    for (const pin of ['gate', 'clockOut']) {
        assert.equal(midi[pin].edges.length, release[pin].edges.length);
        midi[pin].edges.forEach((edge, i) => {
            assert.equal(edge.high, release[pin].edges[i].high);
            assertBetween(ms(midi, edge.cycle) - ms(release, release[pin].edges[i].cycle),
                -0.02, 0.02, `ms between the two images' ${pin} edge ${i}`);
        });
    }
    assert.deepEqual(pitches(midi), pitches(release));
    assert.equal(release.serial.bytes, 0);
    assert.deepEqual(release.midi.bytes, []);
});

test('debug: the log is text, with nothing in it a MIDI input would take as a status', () => {
    const board = createBoard('debug');

    board.setNotes(REST_PANEL);
    board.setTempoReading(READING_150_BPM);
    board.runUntilLines(STEP_COUNT + 1, RUN_LIMIT_MS);

    // A status byte has its top bit set, and no character of the log does
    const lines = [...board.serial.notices, ...board.serial.lines];
    const sent = lines.reduce((count, line) => count + line.text.length + 2, 0);

    assert.equal(sent, board.serial.bytes);
    for (const line of lines) {
        for (const character of line.text) {
            assert.ok(character.charCodeAt(0) < 0x80, `in "${line.text}"`);
        }
    }
    assert.deepEqual(board.midi.bytes, []);
    assert.deepEqual(board.midi.messages, []);
    assert.equal(Math.round(board.machine.usart.baudRate), 117647);
});

// SUSPECTED FIRMWARE GAP, shown here as it stands and not as it should be.
// A watchdog reset that comes while a note is sounding leaves it sounding at
// the receiver: the firmware starts again with no memory of the note and
// sends Start and the first step, and never a Note Off for the note that
// was cut short. The gate output does fall at the reset. When this is put
// right in the firmware, this scenario fails, and its last assertion is the
// one to turn round.
test('midi: KNOWN GAP: a watchdog reset leaves the sounding note with no Note Off', () => {
    const board = midiBoard(PANEL, READING_150_BPM);

    // Step 2's gate has just risen; its Note On still goes, as the loop
    // is still going round, and then nothing but the reset
    board.machine.runUntil(() => board.gate.rises().length >= 3, RUN_LIMIT_MS);
    board.stopTick();
    board.machine.runUntil(() => board.gate.rises().length >= 4, RUN_LIMIT_MS);
    board.runForMs(120);

    assert.equal(board.resets.length, 1);
    assert.deepEqual(withoutClocks(board.midi.messages), [
        'start', `on ${panelNote(PANEL[0])}`, `off ${panelNote(PANEL[0])}`,
        `on ${panelNote(PANEL[1])}`, `off ${panelNote(PANEL[1])}`,
        `on ${panelNote(PANEL[2])}`,
        // The reset is here: the gate fell, and step 2's note was left on
        'start', `on ${panelNote(PANEL[0])}`, `off ${panelNote(PANEL[0])}`,
        `on ${panelNote(PANEL[1])}`,
    ]);
    assert.equal(ofType(board, 'noteOff').some(
        (message) => message.note === panelNote(PANEL[2])), false);
});
