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
//
// The notes and what is sent at all are here; the clock, Start and Stop are
// in midi_clock.test.js, and what the two share in midi_fixture.js.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard, STEP_COUNT } = require('../boards/nano_sequencer');
const { parseMidi, midiNoteName } = require('../boards/midi_messages');
const {
    MIDI_BAUD, BYTE_CYCLES, BYTE_MS, BYTE_GAP_SPARE_MS, START, CLOCK,
    NOTE_ON_CHANNEL_1, VELOCITY, CLOCKS_PER_STEP, UBRR_31250_DOUBLE_SPEED,
    DOUBLE_SPEED_SAMPLES, READING_150_BPM, STEP_MS, HALF_STEP_MS, SETTLE_MS,
    TICK_MS, RUN_LIMIT_MS, NOTE_AFTER_GATE_MIN_MS, NOTE_AFTER_GATE_MAX_MS,
    HEAD_AFTER_PIN_MAX_MS, PANEL, REST_PANEL, MODE_STAGE, MODE_KLEE,
    KLEE_MERGE, KLEE_PATTERN, panelNote, voltageNote, midiBoard, ms, ofType,
    withoutClocks, assertBetween, assertWellFormed, assertNoByteEarly,
    soundingAtEnd, clocksBetween, assertSixClocksAStep,
} = require('./midi_fixture');

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

    // The outputs are written first in a tick, ahead of the MIDI bytes.
    // Each image notices a tick within a pass of its loop, 6 us, and where
    // in the pass moves with the length of the tick before, so the two are
    // not on the same cycle: 9 to 18 us apart measured before MIDI in was
    // read on every tick, 17 to 26 since
    for (const pin of ['gate', 'clockOut']) {
        assert.equal(midi[pin].edges.length, release[pin].edges.length);
        midi[pin].edges.forEach((edge, i) => {
            assert.equal(edge.high, release[pin].edges[i].high);
            assertBetween(ms(midi, edge.cycle) - ms(release, release[pin].edges[i].cycle),
                -0.03, 0.03, `ms between the two images' ${pin} edge ${i}`);
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
