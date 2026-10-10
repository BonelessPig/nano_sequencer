'use strict';
// The release firmware image on the emulated board. It has no log, so these
// scenarios watch the pins and the timer instead: the gate and the clock
// output show when each step begins and how long its note is held.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard, STEP_COUNT } = require('../boards/nano_sequencer');

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
const DDRB = 0x24;              // Port B registers, data addresses
const PORTB = 0x25;
const SS_PIN = 0x04;            // PB2
const LOAD_PIN = 0x02;          // PB1, the shift register load line
const SCK_PIN = 0x20;           // PB5
const OUTPUT_PINS = 0x30;       // PD4 (gate) and PD5 (clock out)
const SPI_HZ = 1000000;
const RUN_LIMIT_MS = 5000;
const PANEL = [1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2, 5, 8, 11, 14];
const REST_STEP = 5;            // The one step of PANEL with no note
const READING_150_BPM = 480;    // Exactly 100 ms a step
const STEP_MS = 100;
const HALF_STEP_MS = 50;        // The gate length and the clock's high time
const TICK_MS = 1;
// Outputs are set first thing in a tick, so their edges land on the tick to
// within the time the idle loop takes to notice it
const EDGE_JITTER_MAX_MS = 0.02;

function assertNear(actual, expected, tolerance, what) {
    assert.ok(Math.abs(actual - expected) <= tolerance,
        `${what}: ${actual} ms, expected ${expected} within ${tolerance}`);
}

// One pass of PANEL at 150 BPM and a little of the next
function playedBoard() {
    const board = createBoard('release');
    board.setNotes(PANEL);
    board.setTempoReading(READING_150_BPM);
    board.runUntilClockOutPulses(STEP_COUNT + 1, RUN_LIMIT_MS);
    return board;
}

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

test('drives only the gate and clock pins of port D', () => {
    const board = releaseBoard();

    board.runUntilPanelReads(2, RUN_LIMIT_MS);

    assert.equal(board.machine.cpu.data[DDRD], OUTPUT_PINS);
});

test('the clock output pulses once a step, high for half of it', () => {
    const board = playedBoard();
    const pulses = board.clockOutPulsesMs();

    assert.equal(pulses.length, STEP_COUNT + 1);
    // The first tick begins step 0 and counts towards step 1
    assertNear(pulses[0].widthMs, HALF_STEP_MS - TICK_MS, EDGE_JITTER_MAX_MS, 'first width');
    assertNear(pulses[1].riseMs - pulses[0].riseMs, STEP_MS - TICK_MS,
        EDGE_JITTER_MAX_MS, 'first period');
    for (let i = 1; i < STEP_COUNT; i++) {
        assertNear(pulses[i].widthMs, HALF_STEP_MS, EDGE_JITTER_MAX_MS, `width ${i}`);
        assertNear(pulses[i + 1].riseMs - pulses[i].riseMs, STEP_MS,
            EDGE_JITTER_MAX_MS, `period ${i}`);
    }
    // No drift: sixteen steps take sixteen step lengths
    assertNear(pulses[STEP_COUNT].riseMs - pulses[0].riseMs,
        (STEP_COUNT * STEP_MS) - TICK_MS, EDGE_JITTER_MAX_MS, 'whole pattern');
});

test('the gate opens for half of every step with a note, and not for a rest', () => {
    const board = playedBoard();
    const clocks = board.clockOutPulsesMs();
    const gates = board.gatePulsesMs();
    const noteSteps = [];

    for (let step = 0; step < STEP_COUNT; step++) {
        if (step !== REST_STEP) {
            noteSteps.push(step);
        }
    }
    noteSteps.push(STEP_COUNT); // Step 0 again, as the pattern wraps

    assert.equal(gates.length, noteSteps.length);
    gates.forEach((gate, i) => {
        const step = noteSteps[i];
        const widthMs = (step === 0) ? HALF_STEP_MS - TICK_MS : HALF_STEP_MS;

        // Each gate rises with its step's clock pulse, a couple of
        // instructions ahead of it
        assertNear(gate.riseMs, clocks[step].riseMs, 0.001, `gate ${step} rise`);
        assert.ok(gate.riseMs < clocks[step].riseMs, `gate ${step} leads the clock`);
        assertNear(gate.widthMs, widthMs, EDGE_JITTER_MAX_MS, `gate ${step} width`);
    });
});

test('the first gate opens on the second tick', () => {
    const board = playedBoard();

    // Worked out on the first tick, 1 ms after reset, and applied on the next
    assertNear(board.gatePulsesMs()[0].riseMs, 2 * TICK_MS, 0.1, 'first gate');
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

test('runs the SPI bus as master in mode 0, MSB first, at 1 MHz', () => {
    const board = releaseBoard();
    const data = board.machine.cpu.data;

    board.runUntilPanelReads(2, RUN_LIMIT_MS);

    assert.equal(board.machine.spi.isMaster, true);
    assert.equal(board.machine.spi.spiMode, 0);
    assert.equal(board.machine.spi.dataOrder, 'msbFirst');
    assert.equal(board.machine.spi.spiFrequency, SPI_HZ);
    // SS, SCK and the load line are outputs, and SS and load idle high
    assert.equal(data[DDRB] & (SS_PIN | SCK_PIN | LOAD_PIN), SS_PIN | SCK_PIN | LOAD_PIN);
    assert.equal(data[PORTB] & (SS_PIN | LOAD_PIN), SS_PIN | LOAD_PIN);
});

test('reads the chain in eight transfers of zeros', () => {
    const board = releaseBoard();

    board.runUntilPanelReads(3, RUN_LIMIT_MS);

    // Two complete reads: eight bytes each, all zeros sent
    assert.deepEqual(board.spiSent.slice(0, 16), new Array(16).fill(0));
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
