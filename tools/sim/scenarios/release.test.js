'use strict';
// The release firmware image on the emulated board. It has no log, so these
// scenarios watch the pins instead.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard, STEP_COUNT } = require('../boards/nano_sequencer');

const CLOCKS_PER_READ = 64;     // 16 steps of 4 bits
const HALF_VOLT = 0.5;          // Reads as 102 of 1023, a 25 ms tempo delay
const HALF_VOLT_DELAY_MS = 25;
const TICK_OVERHEAD_MAX_MS = 1; // Panel read and conversion only
const DDRD = 0x2A;              // Port D direction register, data address
const UART_PINS = 0x03;         // PD0 (receive) and PD1 (transmit)
const RUN_LIMIT_MS = 5000;

function releaseBoard(tempoVolts) {
    const board = createBoard('release');
    board.setTempoVolts(tempoVolts);
    return board;
}

test('never switches the USART on or drives its pins', () => {
    const board = releaseBoard(0);

    board.runUntilPanelReads(STEP_COUNT, RUN_LIMIT_MS);

    assert.equal(board.machine.usart.txEnable, false);
    assert.equal(board.machine.usart.rxEnable, false);
    assert.equal(board.serial.bytes, 0);
    assert.equal(board.machine.cpu.data[DDRD] & UART_PINS, 0);
});

test('reads the panel with 64 clock pulses each time', () => {
    const board = releaseBoard(0);

    board.runUntilPanelReads(STEP_COUNT + 1, RUN_LIMIT_MS);

    for (const read of board.panelReads.slice(0, STEP_COUNT)) {
        assert.equal(read.clocks, CLOCKS_PER_READ);
    }
});

test('a step lasts the tempo delay plus under 1 ms', () => {
    const board = releaseBoard(HALF_VOLT);

    board.runUntilPanelReads(STEP_COUNT, RUN_LIMIT_MS);

    for (const periodMs of board.panelReadPeriodsMs()) {
        assert.ok(periodMs >= HALF_VOLT_DELAY_MS, `${periodMs} ms is too short`);
        assert.ok(periodMs < HALF_VOLT_DELAY_MS + TICK_OVERHEAD_MAX_MS,
            `${periodMs} ms is too long`);
    }
});
