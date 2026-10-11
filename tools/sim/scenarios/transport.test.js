'use strict';
// The clock input, the reset input and the run button on
// the emulated board, for all three firmware images. The release and midi
// images have no log, so steps are timed from the clock output and told
// apart by the voltage the DAC holds as each gate rises; the debug image's
// log lines are checked as well, and the midi image's MIDI in midi.test.js.
//
// The two edge inputs rest on lib/edge_flag.js, which is this project's own
// model of a flag latching with its interrupt off. These scenarios show the
// firmware set the flags up and polls them; they do not show a real chip
// latches them.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard, STEP_COUNT } = require('../boards/nano_sequencer');

const CONFIGS = ['debug', 'release', 'midi'];
// Sixteen notes, no rests, and the first few all different
const PANEL = [3, 6, 9, 12, 15, 2, 5, 8, 11, 14, 1, 4, 7, 10, 13, 2];
const READING_150_BPM = 480;     // Internal clock: exactly 100 ms a step
const READING_ONE_TO_ONE = 500;  // External clock: a step to an edge
const READING_DIVIDE_4 = 0;      // A step to every four edges
const READING_MULTIPLY_4 = 1023; // Four steps to an edge
const STEP_MS = 100;
const TICK_MS = 1;
const RUN_LIMIT_MS = 5000;
const EDGE_JITTER_MAX_MS = 0.02;
const DEBOUNCE_MS = 20;          // A control must hold this long to count
const FIRST_GATE_MS = 10;        // Half of the 20 ms a first step is taken to be
const MILLIVOLTS_PER_OCTAVE = 1000;
const SEMITONES_PER_OCTAVE = 12;
const TCCR1B = 0x81;             // Data addresses of the registers checked
const TIMSK1 = 0x6F;
const EICRA = 0x69;
const EIMSK = 0x3D;
const DDRB = 0x24;
const PORTB = 0x25;
const DDRD = 0x2A;
const PORTD = 0x2B;
const CLOCK_IN_PIN = 0x01;       // PB0
const PORT_D_INPUTS = 0x8C;      // PD2 (reset), PD3 (unused), PD7 (run)

function assertNear(actual, expected, tolerance, what) {
    assert.ok(Math.abs(actual - expected) <= tolerance,
        `${what}: ${actual} ms, expected ${expected} within ${tolerance}`);
}

function noteMillivolts(noteValue) {
    return Math.round(((noteValue - 1) * MILLIVOLTS_PER_OCTAVE) / SEMITONES_PER_OCTAVE);
}

// A board on the internal clock as it powers on, or with external true one
// on the external clock that is waiting for its first edge
function boardWith(config, reading, external) {
    const board = createBoard(config);

    board.setNotes(PANEL);
    board.setTempoReading(reading);
    if (external) {
        board.startOnExternalClock();
    }
    return board;
}

// The step each gate so far belongs to, from the voltage the DAC held as it
// rose. Only steps 0 to 4 of PANEL are asked about: their notes are unique
function stepsPlayed(board) {
    const pitches = PANEL.slice(0, 5).map(noteMillivolts);

    return board.gate.rises().map((rise) => pitches.indexOf(board.cvWriteBefore(rise).millivolts));
}

function riseMs(board, recorder) {
    return recorder.rises().map((cycle) => board.machine.cyclesToMs(cycle));
}

function nowMs(board) {
    return board.machine.cyclesToMs(board.machine.cpu.cycles);
}

for (const config of CONFIGS) {
    test(`${config}: sets up the inputs' pull-ups and both edge flags`, () => {
        const board = boardWith(config, READING_150_BPM, false);
        const data = board.machine.cpu.data;

        board.runForMs(20);

        assert.equal(data[DDRB] & CLOCK_IN_PIN, 0);
        assert.equal(data[PORTB] & CLOCK_IN_PIN, CLOCK_IN_PIN);
        assert.equal(data[DDRD] & PORT_D_INPUTS, 0);
        assert.equal(data[PORTD] & PORT_D_INPUTS, PORT_D_INPUTS);
        // Capture on the falling edge at the pin, noise canceller on, the
        // timer counting; INT0's flag on the falling edge; no interrupt on
        assert.equal(data[TCCR1B], 0x82);
        assert.equal(data[TIMSK1], 0x00);
        assert.equal(data[EICRA], 0x02);
        assert.equal(data[EIMSK], 0x00);
    });

    test(`${config}: a clock edge takes the pattern over, with no restart`, () => {
        const board = boardWith(config, READING_ONE_TO_ONE, false);

        board.runForMs(130);          // Into step 1, at the pot's tempo
        board.startClockIn(80);
        board.runUntilClockOutPulses(7, RUN_LIMIT_MS);

        // From then on a step an edge, carrying on from where it was
        const pulses = board.clockOutPulsesMs();
        for (let i = 4; i < pulses.length; i++) {
            assertNear(pulses[i].riseMs - pulses[i - 1].riseMs, 80,
                EDGE_JITTER_MAX_MS, `period ${i}`);
        }
        assert.deepEqual(stepsPlayed(board).slice(0, 5), [0, 1, 2, 3, 4]);
    });

    test(`${config}: on the external clock a step begins on each clock edge`, () => {
        const board = boardWith(config, READING_ONE_TO_ONE, true);

        // Nothing until the clock comes
        board.runForMs(400);
        assert.equal(board.clockOut.edges.length, 0);
        assert.equal(board.gate.edges.length, 0);

        board.startClockIn(80);
        board.runUntilClockOutPulses(6, RUN_LIMIT_MS);

        const edges = board.clockIn.rises.map((cycle) => board.machine.cyclesToMs(cycle));
        const pulses = board.clockOutPulsesMs();
        const gates = board.gatePulsesMs();

        pulses.forEach((pulse, i) => {
            // The edge is taken on the next tick and its step applied on
            // the one after: between one and two ticks later
            const delayMs = pulse.riseMs - edges[i];

            assert.ok(delayMs > TICK_MS && delayMs < (2 * TICK_MS) + EDGE_JITTER_MAX_MS,
                `step ${i} began ${delayMs} ms after its edge`);
        });
        for (let i = 1; i < pulses.length; i++) {
            assertNear(pulses[i].riseMs - pulses[i - 1].riseMs, 80,
                EDGE_JITTER_MAX_MS, `period ${i}`);
            // Half the 80 ms between the edges before it
            assertNear(pulses[i].widthMs, 40, EDGE_JITTER_MAX_MS, `clock width ${i}`);
            assertNear(gates[i].widthMs, 40, EDGE_JITTER_MAX_MS, `gate width ${i}`);
        }
        // The first step has no spacing to go by
        assertNear(pulses[0].widthMs, FIRST_GATE_MS, EDGE_JITTER_MAX_MS, 'first clock width');
        assertNear(gates[0].widthMs, FIRST_GATE_MS, EDGE_JITTER_MAX_MS, 'first gate width');
        assert.deepEqual(stepsPlayed(board).slice(0, 5), [0, 1, 2, 3, 4]);

        // And when the clock stops, so does the pattern
        board.stopClockIn();
        board.runForMs(100);
        const count = board.clockOut.rises().length;
        board.runForMs(500);
        assert.equal(board.clockOut.rises().length, count);
    });

    test(`${config}: a clock pulse far shorter than a tick is caught`, () => {
        const board = boardWith(config, READING_ONE_TO_ONE, true);

        board.runForMs(10);
        for (let pulse = 0; pulse < 7; pulse++) {
            board.pulseClockIn(0);   // High and low again with no time between
            board.runForMs(13.37);
        }

        assert.equal(board.clockOut.rises().length, 7);
        assert.equal(board.gate.rises().length, 7);
    });

    test(`${config}: a clock that speeds up still gets a pulse and a gate per edge`, () => {
        const board = boardWith(config, READING_ONE_TO_ONE, true);
        const gapsMs = [200, 200, 60, 60, 200, 40, 40];

        board.runForMs(10);
        board.pulseClockIn(1);
        for (const gapMs of gapsMs) {
            board.runForMs(gapMs);
            board.pulseClockIn(1);
        }
        board.runForMs(150);

        // An edge that comes in under half the gap before it finds the gate
        // and clock output still high. Its step waits a tick, with both
        // low, so it is one tick later than the others and none is missing
        const edges = board.clockIn.rises.map((cycle) => board.machine.cyclesToMs(cycle));
        const rises = riseMs(board, board.clockOut);

        assert.equal(rises.length, edges.length);
        assert.equal(board.gate.rises().length, edges.length);
        rises.forEach((rise, i) => {
            const early = (i >= 2) && (gapsMs[i - 1] * 2 < gapsMs[i - 2]);
            const ticks = early ? 2 : 1;
            const delayMs = rise - edges[i];

            assert.ok(delayMs > ticks * TICK_MS
                && delayMs < ((ticks + 1) * TICK_MS) + EDGE_JITTER_MAX_MS,
                `step ${i} began ${delayMs} ms after its edge`);
        });
    });

    test(`${config}: a swung clock gets a step on each of its uneven edges`, () => {
        const board = boardWith(config, READING_ONE_TO_ONE, true);
        const gapsMs = [124, 76];   // 62% and 38% of a pair of 100 ms periods

        board.runForMs(10);
        board.startClockIn(100, 'swing62');
        board.runUntilClockOutPulses(7, RUN_LIMIT_MS);

        const edges = board.clockIn.rises.map((cycle) => board.machine.cyclesToMs(cycle));
        const rises = riseMs(board, board.clockOut);

        rises.forEach((rise, i) => {
            const delayMs = rise - edges[i];

            assert.ok(delayMs > TICK_MS && delayMs < (2 * TICK_MS) + EDGE_JITTER_MAX_MS,
                `step ${i} began ${delayMs} ms after its edge`);
        });
        for (let i = 1; i < rises.length; i++) {
            assertNear(edges[i] - edges[i - 1], gapsMs[(i - 1) % 2],
                EDGE_JITTER_MAX_MS, `edge gap ${i}`);
            assertNear(rises[i] - rises[i - 1], gapsMs[(i - 1) % 2],
                EDGE_JITTER_MAX_MS, `step gap ${i}`);
        }
        assert.deepEqual(stepsPlayed(board).slice(0, 5), [0, 1, 2, 3, 4]);
    });

    test(`${config}: the tempo pot divides the external clock`, () => {
        const board = boardWith(config, READING_DIVIDE_4, true);

        board.runForMs(10);
        board.startClockIn(20);
        board.runUntilClockOutPulses(5, RUN_LIMIT_MS);

        // A step on the first edge and on every fourth after it
        const edges = board.clockIn.rises.map((cycle) => board.machine.cyclesToMs(cycle));
        const pulses = board.clockOutPulsesMs();

        pulses.forEach((pulse, i) => {
            const delayMs = pulse.riseMs - edges[i * 4];

            assert.ok(delayMs > TICK_MS && delayMs < (2 * TICK_MS) + EDGE_JITTER_MAX_MS,
                `step ${i} began ${delayMs} ms after edge ${i * 4}`);
        });
        for (let i = 1; i < pulses.length; i++) {
            assertNear(pulses[i].widthMs, 40, EDGE_JITTER_MAX_MS, `clock width ${i}`);
        }
    });

    test(`${config}: the tempo pot multiplies the external clock`, () => {
        const board = boardWith(config, READING_MULTIPLY_4, true);

        board.runForMs(10);
        board.startClockIn(100);
        board.runUntilClockOutPulses(14, RUN_LIMIT_MS);

        // One step for the first edge, whose spacing is not yet known; from
        // the second edge four steps to an edge, 25 ms apart
        const pulses = board.clockOutPulsesMs();

        assertNear(pulses[1].riseMs - pulses[0].riseMs, 100, EDGE_JITTER_MAX_MS, 'first step');
        for (let i = 2; i < pulses.length; i++) {
            assertNear(pulses[i].riseMs - pulses[i - 1].riseMs, 25,
                EDGE_JITTER_MAX_MS, `period ${i}`);
        }
        assert.deepEqual(stepsPlayed(board).slice(0, 5), [0, 1, 2, 3, 4]);
    });

    test(`${config}: a reset pulse plays the first step at the next step boundary`, () => {
        const board = boardWith(config, READING_150_BPM, false);

        board.runUntilClockOutPulses(3, RUN_LIMIT_MS); // Half way into step 2
        board.pulseResetIn(0);
        board.runUntilClockOutPulses(6, RUN_LIMIT_MS);

        // Step 0 again where step 3 would have been, with the clock not moved
        assert.deepEqual(stepsPlayed(board).slice(0, 6), [0, 1, 2, 0, 1, 2]);
        const pulses = board.clockOutPulsesMs();
        for (let i = 1; i < pulses.length; i++) {
            assertNear(pulses[i].riseMs - pulses[i - 1].riseMs, STEP_MS,
                EDGE_JITTER_MAX_MS, `period ${i}`);
        }
    });

    test(`${config}: a reset on the external clock waits for the next edge`, () => {
        const board = boardWith(config, READING_ONE_TO_ONE, true);

        board.runForMs(10);
        board.startClockIn(60);
        board.runForMs(150);          // Edges at 10, 70 and 130 ms: steps 0 to 2
        board.pulseResetIn(5);
        board.runForMs(120);          // Edges at 190 and 250 ms

        assert.deepEqual(stepsPlayed(board), [0, 1, 2, 0, 1]);
    });

    test(`${config}: the run button stops the pattern and starts it from the top`, () => {
        const board = boardWith(config, READING_150_BPM, false);

        board.runUntilClockOutPulses(2, RUN_LIMIT_MS);
        board.runForMs(55);           // Just into step 2, with its gate open
        assert.deepEqual(stepsPlayed(board), [0, 1, 2]);

        // A press with 5 ms of bounce: it counts 20 ms after it settles,
        // and the note is cut off there, not played out
        const pressedMs = nowMs(board);
        board.pressRunButton(60, 5);
        const cutMs = board.machine.cyclesToMs(board.gate.edges[board.gate.edges.length - 1].cycle);

        assert.equal(board.gate.edges[board.gate.edges.length - 1].high, false);
        assert.ok((cutMs - pressedMs) > DEBOUNCE_MS && (cutMs - pressedMs) < DEBOUNCE_MS + 8,
            `gate cut ${cutMs - pressedMs} ms after the press`);

        // Stopped: nothing rises, however long
        const gateRises = board.gate.rises().length;
        const clockRises = board.clockOut.rises().length;
        board.runForMs(450);
        assert.equal(board.gate.rises().length, gateRises);
        assert.equal(board.clockOut.rises().length, clockRises);
        assert.equal(board.clockOut.edges[board.clockOut.edges.length - 1].high, false);

        // The next press starts step 0, and step 1 is a whole step after it
        const startedMs = nowMs(board);
        board.pressRunButton(60, 5);
        board.runForMs(200);

        const steps = stepsPlayed(board);
        const rises = riseMs(board, board.clockOut);
        const restart = rises[clockRises];

        assert.deepEqual(steps.slice(3, 6), [0, 1, 2]);
        assert.ok((restart - startedMs) > DEBOUNCE_MS && (restart - startedMs) < DEBOUNCE_MS + 8,
            `started ${restart - startedMs} ms after the press`);
        assertNear(rises[clockRises + 1] - restart, STEP_MS, EDGE_JITTER_MAX_MS, 'first step');
        assertNear(rises[clockRises + 2] - rises[clockRises + 1], STEP_MS,
            EDGE_JITTER_MAX_MS, 'second step');
    });

    test(`${config}: a bounce shorter than the debounce time is not a press`, () => {
        const board = boardWith(config, READING_150_BPM, false);

        board.runForMs(120);
        board.pressRunButton(15);     // Held for less than it takes to count
        board.runUntilClockOutPulses(6, RUN_LIMIT_MS);

        const pulses = board.clockOutPulsesMs();
        for (let i = 1; i < pulses.length; i++) {
            assertNear(pulses[i].riseMs - pulses[i - 1].riseMs, STEP_MS,
                EDGE_JITTER_MAX_MS, `period ${i}`);
        }
    });

    test(`${config}: the pot sets the tempo only once it has met it again`, () => {
        const board = boardWith(config, READING_150_BPM, false);
        const lastPeriodMs = () => {
            const rises = riseMs(board, board.clockOut);

            return rises[rises.length - 1] - rises[rises.length - 2];
        };

        // The selector off the tempo and the pot turned right up: it holds
        board.runForMs(250);
        board.setSelector(1);
        board.setTempoReading(1023);
        board.runUntilClockOutPulses(8, RUN_LIMIT_MS);
        assertNear(lastPeriodMs(), STEP_MS, EDGE_JITTER_MAX_MS, 'off the tempo');

        // Back on it, with the pot not where the tempo was left: it holds
        board.setSelector(0);
        board.runUntilClockOutPulses(12, RUN_LIMIT_MS);
        assertNear(lastPeriodMs(), STEP_MS, EDGE_JITTER_MAX_MS, 'not yet met');

        // Turned back to meet it, and then up: 285 BPM is 52.6 ms a step
        board.setTempoReading(READING_150_BPM);
        board.runForMs(30);
        board.setTempoReading(1023);
        board.runUntilClockOutPulses(18, RUN_LIMIT_MS);
        assertNear(lastPeriodMs(), 15000 / 285, 1.1, 'met and turned up');
    });

    test(`${config}: a stop and a start go back to the internal clock`, () => {
        const board = boardWith(config, READING_150_BPM, true);

        // One edge, one step, and nothing more however long the wait
        board.pulseClockIn(2);
        board.runForMs(50);
        board.runForMs(400);
        assert.equal(board.clockOut.rises().length, 1);

        // The start is from the first step, at the tempo
        board.pressRunButton(60);
        board.runForMs(60);
        board.pressRunButton(60);
        board.runUntilClockOutPulses(5, RUN_LIMIT_MS);
        const rises = riseMs(board, board.clockOut);

        assert.deepEqual(stepsPlayed(board).slice(0, 5), [0, 0, 1, 2, 3]);
        assertNear(rises[4] - rises[3], STEP_MS, EDGE_JITTER_MAX_MS,
            'a step at the tempo');
    });
}

test('debug: each change of transport is logged, with its number', () => {
    const board = boardWith('debug', READING_150_BPM, false);

    board.runForMs(150);
    board.pressRunButton(60);
    board.runForMs(100);
    board.pressRunButton(60);
    board.runForMs(50);
    board.pulseClockIn(2);
    board.runForMs(100);

    const notices = board.serial.notices.map((line) => line.text);

    assert.deepEqual(notices.slice(1),
        ['Transport = 0', 'Transport = 1', 'Transport = 2']);
    assert.ok(notices[0].startsWith('Reset = '));
});

test('debug: a start is logged ahead of its first step', () => {
    const board = boardWith('debug', READING_150_BPM, false);

    board.runForMs(150);
    board.pressRunButton(60);
    board.runForMs(100);
    const linesBefore = board.serial.lines.length;
    board.pressRunButton(60);
    board.runForMs(50);

    const started = board.serial.notices[board.serial.notices.length - 1];
    const firstStep = board.serial.lines[linesBefore];

    assert.equal(started.text, 'Transport = 1');
    assert.equal(firstStep.text, `Step 0 Note = ${PANEL[0] - 1}`);
    assert.ok(started.cycle < firstStep.startCycle);
});

test('debug: a start on the external clock is logged and no step is', () => {
    const board = boardWith('debug', READING_ONE_TO_ONE, true);

    board.runForMs(300);

    assert.deepEqual(board.serial.notices.map((line) => line.text), ['Transport = 2']);
    assert.equal(board.serial.lines.length, 0);
    assert.equal(STEP_COUNT, PANEL.length);
});
