'use strict';
// The debug firmware image on the emulated board: what it logs, how it
// drives the shift registers, and how long a step takes.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard, STEP_COUNT } = require('../boards/nano_sequencer');

const PANEL = [1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2, 5, 8, 11, 14];
const CLOCKS_PER_READ = 64;    // 16 steps of 4 bits
const HALF_VOLT = 0.5;         // Reads as 102 of 1023, a 25 ms tempo delay
const HALF_VOLT_DELAY_MS = 25;
const TICK_OVERHEAD_MAX_MS = 2; // Panel read, conversion and one log line
const RUN_LIMIT_MS = 5000;

function debugBoard(tempoVolts) {
    const board = createBoard('debug');
    board.setNotes(PANEL);
    board.setTempoVolts(tempoVolts);
    return board;
}

test('logs every step with the note set on the panel, and wraps', () => {
    const board = debugBoard(0);
    const lineCount = (STEP_COUNT * 2) + 1;

    board.runUntilLines(lineCount, RUN_LIMIT_MS);

    board.serial.lines.forEach((line, i) => {
        const step = i % STEP_COUNT;
        assert.equal(line.text, `Step ${step} Note = ${PANEL[step]}`);
    });
});

test('sends at 115200 baud in double-speed mode', () => {
    const board = debugBoard(0);

    board.runUntilLines(1, RUN_LIMIT_MS);

    assert.equal(board.machine.usart.txEnable, true);
    assert.equal(Math.round(board.machine.usart.baudRate), 117647);
});

test('reads the panel once per step, with 64 clock pulses', () => {
    const board = debugBoard(0);

    board.runUntilLines(STEP_COUNT, RUN_LIMIT_MS);

    assert.equal(board.panelReads.length, STEP_COUNT);
    for (const read of board.panelReads) {
        assert.equal(read.clocks, CLOCKS_PER_READ);
    }
});

test('a switch changed mid-pattern is heard when its step next plays', () => {
    const board = debugBoard(0);
    const changed = PANEL.slice();
    changed[2] = 9;

    board.runUntilLines(STEP_COUNT, RUN_LIMIT_MS);
    board.setNotes(changed);
    board.runUntilLines(STEP_COUNT * 2, RUN_LIMIT_MS);

    assert.equal(board.serial.lines[2].text, 'Step 2 Note = 7');
    assert.equal(board.serial.lines[STEP_COUNT + 2].text, 'Step 2 Note = 9');
});

test('a step lasts the tempo delay plus under 2 ms', () => {
    const board = debugBoard(HALF_VOLT);

    board.runUntilPanelReads(STEP_COUNT, RUN_LIMIT_MS);

    for (const periodMs of board.panelReadPeriodsMs()) {
        assert.ok(periodMs >= HALF_VOLT_DELAY_MS, `${periodMs} ms is too short`);
        assert.ok(periodMs < HALF_VOLT_DELAY_MS + TICK_OVERHEAD_MAX_MS,
            `${periodMs} ms is too long`);
    }
});
