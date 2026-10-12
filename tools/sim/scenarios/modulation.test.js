'use strict';
// The modulation input (an LFO) and the four things it moves, each by the
// amount the pot gave it with the selector on its position, on the emulated
// board, for all three firmware images. Steps are told apart by the voltage
// the DAC holds as each gate rises, and timed from the pins.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard } = require('../boards/nano_sequencer');

const CONFIGS = ['debug', 'release', 'midi'];
const NOTE = 5;                  // On every step
const PANEL = new Array(16).fill(NOTE);
const READING_150_BPM = 480;     // Exactly 100 ms a step
const STEP_MS = 100;
const PULSE_MS = STEP_MS / 24;   // A clock pulse: gate lengths are counted in them
const TOP = 1023;                // Of the pot and of the LFO
const LFO_CENTRE = 512;
const LFO_HALF_UP = 768;
const POSITION_NOTE = 1;         // Selector positions
const POSITION_GATE = 2;
const POSITION_CHANCE = 3;
const POSITION_STEP = 4;
const STEP_COUNT = 16;
// Step n holds the note n + 1, so a pitch says which step played it; the
// last two share the top note
const COUNTING_PANEL = PANEL.map((note, step) => Math.min(step + 1, 15));
const SCAN_MS = 20;              // More than two input scans
const MILLIVOLTS_PER_OCTAVE = 1000;
const SEMITONES_PER_OCTAVE = 12;
const MIDI_LOWEST_NOTE = 24;     // The MIDI note of 0 V

function noteMillivolts(noteValue) {
    return Math.round(((noteValue - 1) * MILLIVOLTS_PER_OCTAVE) / SEMITONES_PER_OCTAVE);
}

function playingBoard(config) {
    const board = createBoard(config);

    board.setNotes(PANEL);
    board.setTempoReading(READING_150_BPM);
    board.runForMs(SCAN_MS);
    return board;
}

// Gives one position its amount: the pot down to meet the 0 it holds, and up
function setAmount(board, position, reading) {
    board.setSelector(position);
    board.setTempoReading(0);
    board.runForMs(SCAN_MS);
    board.setTempoReading(reading);
    board.runForMs(SCAN_MS);
}

// Sets the LFO, lets a scan take it, and runs on for a number of steps.
// Gives what happened from then: the pitch each gate rose on, the gates
// that finished, and how often the clock output and the gate rose
function playWithLfo(board, reading, steps) {
    board.setLfoReading(reading);
    board.runForMs(SCAN_MS);

    const from = board.machine.cpu.cycles;
    const fromMs = board.machine.cyclesToMs(from);

    board.runForMs(steps * STEP_MS);

    const gateRises = board.gate.rises().filter((cycle) => cycle > from);

    return {
        from,
        pitches: gateRises.map((cycle) => board.cvWriteBefore(cycle).millivolts),
        gates: board.gatePulsesMs().filter((pulse) => pulse.riseMs > fromMs),
        gateRises: gateRises.length,
        gateEdges: board.gate.edges.filter((edge) => edge.cycle > from).length,
        clockRises: board.clockOut.rises().filter((cycle) => cycle > from).length,
    };
}

function assertAllEqual(values, expected, what) {
    assert.ok(values.length > 0, `${what}: nothing to check`);
    assert.deepEqual(values, new Array(values.length).fill(expected), what);
}

for (const config of CONFIGS) {
    test(`${config}: with the LFO at its centre the amounts move nothing`, () => {
        const board = playingBoard(config);

        setAmount(board, POSITION_NOTE, TOP);
        setAmount(board, POSITION_GATE, TOP);
        setAmount(board, POSITION_CHANCE, TOP);

        const played = playWithLfo(board, LFO_CENTRE, 8);

        assert.equal(played.clockRises, 8);
        assert.equal(played.gateRises, 8);
        assertAllEqual(played.pitches, noteMillivolts(NOTE), 'pitches');
        for (const gate of played.gates) {
            assert.ok(Math.abs(gate.widthMs - (STEP_MS / 2)) < 0.1, `gate of ${gate.widthMs} ms`);
        }
    });

    test(`${config}: the LFO moves the note by the amount at position 1`, () => {
        const board = playingBoard(config);

        // No amount yet: the LFO at its top moves nothing
        assertAllEqual(playWithLfo(board, TOP, 3).pitches, noteMillivolts(NOTE), 'no amount');

        // Seven places up at its top, four half way, and seven down at its
        // bottom, which stops at the lowest note
        setAmount(board, POSITION_NOTE, TOP);
        assertAllEqual(playWithLfo(board, TOP, 3).pitches, noteMillivolts(NOTE + 7), 'top');
        if (board.sendsMidi) {
            const notes = board.midi.messages.filter((message) => message.type === 'noteOn');

            assert.equal(notes[notes.length - 1].note,
                MIDI_LOWEST_NOTE + (NOTE + 7) - 1);
        }
        assertAllEqual(playWithLfo(board, LFO_HALF_UP, 3).pitches,
            noteMillivolts(NOTE + 4), 'half way up');
        assertAllEqual(playWithLfo(board, 0, 3).pitches, noteMillivolts(1), 'bottom');

        // Half the amount is half as far: just under three and a half
        // places at the top, which is three
        board.setTempoReading(511);
        board.runForMs(SCAN_MS);
        assertAllEqual(playWithLfo(board, TOP, 3).pitches,
            noteMillivolts(NOTE + 3), 'half the amount');
    });

    test(`${config}: the LFO moves the gate length by the amount at position 2`, () => {
        const board = playingBoard(config);

        setAmount(board, POSITION_GATE, TOP);

        // Half way up: 18 pulses in place of 12
        for (const gate of playWithLfo(board, LFO_HALF_UP, 4).gates) {
            assert.ok(Math.abs(gate.widthMs - (18 * PULSE_MS)) < 1.1, `gate of ${gate.widthMs} ms`);
        }

        // At the bottom: one pulse, the least there is
        const short = playWithLfo(board, 0, 4);

        assert.equal(short.gateRises, 4);
        for (const gate of short.gates) {
            assert.ok(Math.abs(gate.widthMs - PULSE_MS) < 1.1, `gate of ${gate.widthMs} ms`);
        }

        // At the top: a whole step, so each note is tied to the next. Once
        // the first of them has begun the gate never moves
        board.setLfoReading(TOP);
        board.runForMs(SCAN_MS + STEP_MS);

        const tied = playWithLfo(board, TOP, 4);

        assert.equal(tied.clockRises, 4);
        assert.equal(tied.gateEdges, 0);
        assert.equal(board.gate.edges[board.gate.edges.length - 1].high, true);
    });

    test(`${config}: the LFO makes steps drop out by the amount at position 3`, () => {
        const board = playingBoard(config);

        setAmount(board, POSITION_CHANCE, TOP);

        // At the top a step drops out on any draw but 0, and none of the
        // first random numbers is 0. The clock goes on
        const dropped = playWithLfo(board, TOP, 6);

        assert.equal(dropped.clockRises, 6);
        assert.equal(dropped.gateRises, 0);
        if (board.hasLog) {
            const lines = board.serial.lines.slice(-6).map((line) => line.text);

            assert.ok(lines.every((text) => /^Step \d+ Rest$/.test(text)), lines.join(', '));
        }

        // Below its centre nothing drops out
        const back = playWithLfo(board, 0, 6);

        assert.equal(back.gateRises, 6);
        assertAllEqual(back.pitches, noteMillivolts(NOTE), 'pitches');
    });

    test(`${config}: the LFO moves the step played by the amount at position 4`, () => {
        const board = createBoard(config);

        board.setNotes(COUNTING_PANEL);
        board.setTempoReading(READING_150_BPM);
        board.runForMs(SCAN_MS);
        setAmount(board, POSITION_STEP, TOP);

        // The step the pattern has got to is counted from the clock output,
        // which rises once a step, after the gate. Half way up the LFO
        // plays the step four places on, and at its bottom eight back; at
        // its centre again, the step the pattern is on, which never moved
        for (const [reading, places] of [[LFO_CENTRE, 0], [LFO_HALF_UP, 4], [0, 8],
            [LFO_CENTRE, 0]]) {
            const played = playWithLfo(board, reading, 3);
            const clockRises = board.clockOut.rises();
            const expected = board.gate.rises()
                .filter((cycle) => cycle > played.from)
                .map((cycle) => clockRises.filter((rise) => rise < cycle).length)
                .map((step) => COUNTING_PANEL[(step + places) % STEP_COUNT])
                .map(noteMillivolts);

            assert.equal(played.gateRises, 3, `gates with the LFO at ${reading}`);
            assert.deepEqual(played.pitches, expected, `pitches with the LFO at ${reading}`);
        }
    });
}
