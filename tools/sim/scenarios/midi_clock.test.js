'use strict';
// MIDI out on the emulated board: the clock, Start and Stop, on the
// internal clock and an external one, and what is kept when there is more
// to send than the line can carry. midi.test.js has the notes and says
// what a byte's cycle is and what is not modelled.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { STEP_COUNT } = require('../boards/nano_sequencer');
const {
    BYTE_MS, CLOCKS_PER_STEP, STEPS_PER_QUARTER, CLOCKS_PER_QUARTER,
    READING_150_BPM, READING_ONE_TO_ONE, READING_MULTIPLY_4, STEP_MS,
    HALF_STEP_MS, SETTLE_MS, CLOCK_MS, CLOCK_JITTER_MAX_MS, TICK_MS,
    RUN_LIMIT_MS, DEBOUNCE_MS, NOTE_AFTER_GATE_MAX_MS, HEAD_AFTER_PIN_MAX_MS,
    PANEL, panelNote, voltageNote, midiBoard, ms, ofType, withoutClocks,
    assertBetween, assertWellFormed, assertNoByteEarly, soundingAtEnd,
    clocksBetween, assertSixClocksAStep, clockPeriodsMs,
} = require('./midi_fixture');

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

    // Stopped, with no external edge since: no step and no note, and the
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

test('midi: on the external clock Start goes at once and the clocks wait for an edge', () => {
    const board = midiBoard(PANEL, READING_ONE_TO_ONE, true);

    // Started with no clock coming: Start, and then nothing however long
    board.runForMs(400);
    assert.deepEqual(board.midi.messages.map((message) => message.type), ['start']);

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

test('midi: stopped after an external edge sends no clocks, whatever comes in', () => {
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
