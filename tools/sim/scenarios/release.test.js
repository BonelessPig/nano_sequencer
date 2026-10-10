'use strict';
// The release firmware image on the emulated board. It has no log, so these
// scenarios watch the pins and the timer instead. Until there is a gate
// output, nothing outside the chip shows when a step begins in this build;
// what can be seen is that its tick runs at the same 1 kHz as the debug one.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard } = require('../boards/nano_sequencer');

const CLOCKS_PER_READ = 64;     // 16 steps of 4 bits
const SCAN_PERIOD_MS = 8;       // The panel and pot are read every 8 ticks
const SCAN_JITTER_MAX_MS = 0.01; // Nothing else happens at the tick boundary
const FASTEST_READING = 1023;
const DDRD = 0x2A;              // Port D direction register, data address
const UART_PINS = 0x03;         // PD0 (receive) and PD1 (transmit)
const SREG = 0x5F;              // Status register, data address
const SREG_INTERRUPTS_ON = 0x80;
const TIMSK2 = 0x70;            // Timer/Counter2 registers, data addresses
const TCCR2A = 0xB0;
const TCCR2B = 0xB1;
const OCR2A = 0xB3;
const RUN_LIMIT_MS = 5000;

function releaseBoard() {
    const board = createBoard('release');
    board.setTempoReading(FASTEST_READING);
    return board;
}

test('never switches the USART on or drives its pins', () => {
    const board = releaseBoard();

    board.runUntilPanelReads(100, RUN_LIMIT_MS);

    assert.equal(board.machine.usart.txEnable, false);
    assert.equal(board.machine.usart.rxEnable, false);
    assert.equal(board.serial.bytes, 0);
    assert.equal(board.machine.cpu.data[DDRD] & UART_PINS, 0);
});

test('sets Timer/Counter2 for a 1 kHz interrupt and enables interrupts', () => {
    const board = releaseBoard();
    const data = board.machine.cpu.data;

    board.runUntilPanelReads(2, RUN_LIMIT_MS);

    assert.equal(data[OCR2A], 249);  // 16 MHz / 64 / 250
    assert.equal(data[TCCR2A], 0x02); // Clear on compare match
    assert.equal(data[TCCR2B], 0x04); // System clock / 64
    assert.equal(data[TIMSK2], 0x02); // Compare match A interrupt
    assert.equal(data[SREG] & SREG_INTERRUPTS_ON, SREG_INTERRUPTS_ON);
});

test('reads the panel every 8 ms, with 64 clock pulses', () => {
    const board = releaseBoard();
    const readCount = 200;

    board.runUntilPanelReads(readCount + 1, RUN_LIMIT_MS);

    for (const read of board.panelReads.slice(0, readCount)) {
        assert.equal(read.clocks, CLOCKS_PER_READ);
    }
    for (const periodMs of board.panelReadPeriodsMs()) {
        assert.ok(Math.abs(periodMs - SCAN_PERIOD_MS) <= SCAN_JITTER_MAX_MS,
            `panel read period ${periodMs} ms`);
    }
});

test('the first panel read is on the first tick', () => {
    const board = releaseBoard();

    board.runUntilPanelReads(1, RUN_LIMIT_MS);

    const firstMs = board.machine.cyclesToMs(board.panelReads[0].cycle);
    assert.ok(firstMs < 2, `first panel read at ${firstMs} ms`);
});
