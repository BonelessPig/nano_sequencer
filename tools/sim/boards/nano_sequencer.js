'use strict';
/**
 * The sequencer board as it is wired today: an ATmega328P at 16 MHz with
 * eight 74HC165s on the SPI bus (clock on SCK, data on MISO) and their load
 * line on PB1, the tempo pot on ADC channel 6, the gate output on PD4, the
 * clock output on PD5, and USART0 going to a PC. Scenarios set the panel and
 * the pot, run the firmware, and read back what it did. Nothing is wired to
 * Timer/Counter2; the firmware uses it for its 1 kHz tick.
 */
const path = require('node:path');
const { createAtmega328p } = require('../lib/machine');
const { attachPinRecorder } = require('../lib/pin_recorder');
const { attachHc165Chain } = require('../parts/hc165');

const CLOCK_HZ = 16000000;
const STEP_COUNT = 16;
const NOTE_BITS = 4;
const TEMPO_ADC_CHANNEL = 6;
const ADC_REFERENCE_VOLTS = 5;
const ADC_STEPS = 1024;
const SHIFT_REG_LOAD_PIN = 1; // On port B
const GATE_PIN = 4;           // On port D
const CLOCK_OUT_PIN = 5;      // On port D
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
    const spiSent = [];
    let partial = '';
    let partialStart = 0;

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

    const chain = attachHc165Chain(machine.cpu, machine.portB, SHIFT_REG_LOAD_PIN, panelBits);
    const gate = attachPinRecorder(machine.cpu, machine.portD, GATE_PIN);
    const clockOut = attachPinRecorder(machine.cpu, machine.portD, CLOCK_OUT_PIN);

    // A recorder's completed pulses, in milliseconds of chip time
    function pulsesMs(recorder) {
        return recorder.pulses().map((pulse) => ({
            riseMs: machine.cyclesToMs(pulse.rise),
            widthMs: machine.cyclesToMs(pulse.width),
        }));
    }

    // The chain is the only thing on the bus: every transfer clocks it eight
    // times, and the byte it gives back arrives when the transfer ends
    machine.spi.onByte = (sent) => {
        const received = chain.shiftByte();

        spiSent.push(sent);
        machine.cpu.addClockEvent(
            () => machine.spi.completeTransfer(received), machine.spi.transferCycles);
    };

    machine.usart.onByteTransmit = (byte) => {
        serial.bytes++;
        if (partial === '') {
            partialStart = machine.cpu.cycles;
        }
        partial += String.fromCharCode(byte);
        if (partial.endsWith('\r\n')) {
            serial.lines.push({
                text: partial.slice(0, -2),
                startCycle: partialStart,
                cycle: machine.cpu.cycles,
            });
            partial = '';
        }
    };

    return {
        machine,
        serial,
        panelReads: chain.reads,
        spiSent,
        gate,
        clockOut,

        /** Completed gate pulses: when each rose and how long it was high. */
        gatePulsesMs() {
            return pulsesMs(gate);
        },

        /** Completed clock output pulses, in the same form. */
        clockOutPulsesMs() {
            return pulsesMs(clockOut);
        },

        /** Runs until the clock output has finished this many pulses. */
        runUntilClockOutPulses(count, limitMs) {
            machine.runUntil(() => clockOut.pulses().length >= count, limitMs);
        },

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

        /**
         * Sets the tempo pot so the firmware reads exactly this value,
         * 0 to 1023 (the middle of that reading's voltage band).
         */
        setTempoReading(reading) {
            const volts = ((reading + 0.5) * ADC_REFERENCE_VOLTS) / ADC_STEPS;
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

        /**
         * Milliseconds of chip time from the first byte of each log line to
         * the first byte of the next.
         */
        lineStartPeriodsMs() {
            const periods = [];
            for (let i = 1; i < serial.lines.length; i++) {
                const cycles = serial.lines[i].startCycle - serial.lines[i - 1].startCycle;
                periods.push(machine.cyclesToMs(cycles));
            }
            return periods;
        },
    };
}

module.exports = { createBoard, STEP_COUNT };
