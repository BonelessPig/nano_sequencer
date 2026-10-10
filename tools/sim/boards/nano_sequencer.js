'use strict';
/**
 * The sequencer board as it is wired today: an ATmega328P at 16 MHz with
 * eight 74HC165s on PD2 (load), PD3 (clock) and PD4 (data), the tempo pot on
 * ADC channel 6, and USART0 going to a PC. Scenarios set the panel and the
 * pot, run the firmware, and read back what it did.
 */
const path = require('node:path');
const { createAtmega328p } = require('../lib/machine');
const { attachHc165Chain } = require('../parts/hc165');

const CLOCK_HZ = 16000000;
const STEP_COUNT = 16;
const NOTE_BITS = 4;
const TEMPO_ADC_CHANNEL = 6;
const SHIFT_REG_PINS = { load: 2, clock: 3, data: 4 };
const BUILD_DIR = path.resolve(__dirname, '..', '..', '..', 'build');

/**
 * Powers up a board running one build of the firmware.
 * @param config  'debug' or 'release': which build/<config>/output.hex to run.
 */
function createBoard(config) {
    const hexPath = path.join(BUILD_DIR, config, 'output.hex');
    const machine = createAtmega328p(hexPath, CLOCK_HZ);
    const notes = new Array(STEP_COUNT).fill(0);
    const serial = { bytes: 0, lines: [] };
    let partial = '';

    // Step 0's most significant bit is the first bit out of the chain
    function panelBits() {
        const bits = [];
        for (const note of notes) {
            for (let bit = NOTE_BITS - 1; bit >= 0; bit--) {
                bits.push((note >> bit) & 1);
            }
        }
        return bits;
    }

    const chain = attachHc165Chain(machine.cpu, machine.portD, SHIFT_REG_PINS, panelBits);

    machine.usart.onByteTransmit = (byte) => {
        serial.bytes++;
        partial += String.fromCharCode(byte);
        if (partial.endsWith('\r\n')) {
            serial.lines.push({ text: partial.slice(0, -2), cycle: machine.cpu.cycles });
            partial = '';
        }
    };

    return {
        machine,
        serial,
        panelReads: chain.reads,

        /** Sets all 16 step switches; each note is 0 to 15. */
        setNotes(newNotes) {
            for (let step = 0; step < STEP_COUNT; step++) {
                notes[step] = newNotes[step];
            }
        },

        /** Sets the tempo pot wiper voltage, 0 to 5 V. */
        setTempoVolts(volts) {
            machine.adc.channelValues[TEMPO_ADC_CHANNEL] = volts;
        },

        /** Runs until the firmware has read the panel this many times. */
        runUntilPanelReads(count, limitMs) {
            machine.runUntil(() => chain.reads.length >= count, limitMs);
        },

        /** Runs until this many complete log lines have been sent. */
        runUntilLines(count, limitMs) {
            machine.runUntil(() => serial.lines.length >= count, limitMs);
        },

        /** Milliseconds of chip time between consecutive panel reads. */
        panelReadPeriodsMs() {
            const periods = [];
            for (let i = 1; i < chain.reads.length; i++) {
                const cycles = chain.reads[i].cycle - chain.reads[i - 1].cycle;
                periods.push(machine.cyclesToMs(cycles));
            }
            return periods;
        },
    };
}

module.exports = { createBoard, STEP_COUNT };
