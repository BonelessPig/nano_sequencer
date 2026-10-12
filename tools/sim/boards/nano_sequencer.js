'use strict';
/**
 * The sequencer board as it is wired today: an ATmega328P at 16 MHz with
 * ten 74HC165s on the SPI bus (clock on SCK, data on MISO) and their load
 * line on PB1: eight for the 16 steps, one for the eight mode switches and,
 * farthest from the MCU, one whose low three inputs are the selector, a
 * coded rotary switch that says what the pot sets. An MCP4822 DAC on the same bus (clock on SCK, data on MOSI)
 * with its chip select on PB2, two 74HC595s on the bus as well (clock on
 * SCK, data on MOSI) driving the sixteen step lights, their latch clock on
 * PB1 with the 74HC165 load line, the tempo pot on ADC channel 6, the gate
 * output on PD4, the clock output on PD5, and USART0, whose transmit pin (PD1)
 * goes to a PC in the debug image, to the MIDI out socket in the midi image
 * and nowhere in the release image. Scenarios set the panel and
 * the pot, run the firmware, and read back what it did. Nothing is wired to
 * Timer/Counter2; the firmware uses it for its 1 kHz tick. The chip's own
 * EEPROM holds the pitch calibration record, from address 0, and the crash
 * log, from address 16. The chip's watchdog is running; stopTick() is how a
 * scenario makes it bite.
 *
 * Three inputs, each with the chip's pull-up holding its pin high. The clock
 * input jack reaches PB0 and the reset input jack PD2 through a transistor
 * each, which pulls the pin low while the jack is high. The run button is
 * between PD7 and ground, so it reads low while closed. PD3 has nothing on
 * it.
 */
const path = require('node:path');
const { attachDutyMeter } = require('../lib/duty_meter');
const { createAtmega328p } = require('../lib/machine');
const { attachPinRecorder } = require('../lib/pin_recorder');
const { CLOCK_PATTERNS, makeClockPattern } = require('../parts/clock_patterns');
const { attachClockSource } = require('../parts/clock_source');
const { attachHc165Chain } = require('../parts/hc165');
const { attachHc595Chain } = require('../parts/hc595');
const { attachMcp4822 } = require('../parts/mcp4822');
const { attachSwitch } = require('../parts/switch');
const { readCrashLog, CRASH_LOG_ADDRESS, CRASH_LOG_BYTES } = require('./crash_record');
const { createMidiParser } = require('./midi_messages');
const WIRING = require('./wiring');

const CLOCK_HZ = 16000000;
const STEP_COUNT = 16;
const NOTE_BITS = 4;
const MODE_BITS = 8;
const BITS_PER_CHIP = 8;
const SELECTOR_POSITIONS = 8; // The low three inputs of the tenth chip
const LED_CHIPS = 2;
const TEMPO_ADC_CHANNEL = WIRING.analog.pot.channel;
// The modulation input: an LFO, through its input stage
const MOD_ADC_CHANNEL = WIRING.analog.modulation.channel;
const MOD_CENTRE_READING = 512; // What it reads with nothing plugged in
// The rate pot of the sequencer's own LFO, which is off with it right down
const RATE_ADC_CHANNEL = WIRING.analog.rate.channel;
const ADC_REFERENCE_VOLTS = 5;
const ADC_STEPS = 1024;
// Which pin each wire is on comes from wiring.js, which the schematic page
// reads as well
const PINS = WIRING.pins;
const MCUSR = 0x54;           // Reset flags register, data address
const SREG = 0x5F;            // Status register, data address
const INTERRUPT_ENABLE = 0x80; // Its global interrupt enable bit
const STEP_LINE_PREFIX = 'Step ';
const BUILD_DIR = path.resolve(__dirname, '..', '..', '..', 'build');
// The firmware images, and what each sends from the USART's transmit pin:
// a log in text, MIDI, or nothing, the USART never being switched on
const WIRE_LOG = 'log';
const WIRE_MIDI = 'midi';
const WIRE_NONE = 'none';
const WIRES = { debug: WIRE_LOG, release: WIRE_NONE, midi: WIRE_MIDI };
const CONFIGS = Object.keys(WIRES);

// The panel that asks for calibration mode at power-on: the last four steps
// read 10, 5, 10, 5. The lowest switch of the last step is the store switch
const CAL_ENTRY_NOTES = [10, 5, 10, 5];
const CAL_STORE_SWITCH = 1;

/**
 * Powers up a board running one build of the firmware.
 * @param config   One of CONFIGS ('debug', 'release' or 'midi'): which
 *                 build/<config>/output.hex to run. It also says what the
 *                 bytes the USART sends are taken to be: the debug image's
 *                 are sorted into the log's lines (serial.lines and
 *                 serial.notices) and the midi image's are kept as they are
 *                 (midi.bytes) and read into messages (midi.messages).
 * @param options  Optional. eeprom: bytes the EEPROM holds from address 0,
 *                 the rest being erased. resetFlags: what the chip's reset
 *                 flags register holds at start (1 for a power-on reset);
 *                 the emulator leaves it at 0.
 */
function createBoard(config, options = {}) {
    if (WIRES[config] === undefined) {
        throw new Error(`no firmware image called "${config}"`);
    }
    const hexPath = path.join(BUILD_DIR, config, 'output.hex');
    const machine = createAtmega328p(hexPath, CLOCK_HZ);
    const notes = new Array(STEP_COUNT).fill(0);
    let modeSwitches = 0;
    let selector = 0;
    // lines holds the step lines, which most scenarios count and time;
    // notices holds every other line (start-up, calibration, errors)
    // bytes counts everything the USART sent, whatever it was
    const serial = { bytes: 0, lines: [], notices: [] };
    // What the midi image sent: bytes holds each byte as { value, cycle },
    // the cycle being the one in which the firmware handed it to the USART
    // and its start bit began, and messages the same read into MIDI
    // messages by midi_messages.js, a message's cycle being that of its
    // first byte. Both stay empty on the other images
    const midi = { bytes: [], messages: [] };
    const midiParser = createMidiParser();
    const spiSent = [];
    let partial = '';
    let partialStart = 0;

    machine.eepromMemory.set(options.eeprom || []);
    machine.cpu.data[MCUSR] = options.resetFlags || 0;

    // The emulated port a wire's pin is on
    function portOf(pin) {
        return machine[`port${pin.port}`];
    }

    // Step 0's most significant bit is the first bit out of the chain, the
    // mode switches follow the last step, highest switch first, and then
    // the selector's chip, whose five spare inputs are low
    function panelBits() {
        const bits = [];
        for (const note of notes) {
            for (let bit = NOTE_BITS - 1; bit >= 0; bit--) {
                bits.push((note >> bit) & 1);
            }
        }
        for (let bit = MODE_BITS - 1; bit >= 0; bit--) {
            bits.push((modeSwitches >> bit) & 1);
        }
        for (let bit = BITS_PER_CHIP - 1; bit >= 0; bit--) {
            bits.push((selector >> bit) & 1);
        }
        return bits;
    }

    const chain = attachHc165Chain(machine.cpu, portOf(PINS.load), PINS.load.bit, panelBits);
    const dac = attachMcp4822(machine.cpu, portOf(PINS.dacSelect), PINS.dacSelect.bit);
    // The step lights: bit n of the chain's outputs is the light of step n
    const leds = attachHc595Chain(machine.cpu, portOf(PINS.load), PINS.load.bit, LED_CHIPS);
    const gate = attachPinRecorder(machine.cpu, portOf(PINS.gate), PINS.gate.bit);
    const clockOut = attachPinRecorder(machine.cpu, portOf(PINS.clockOut), PINS.clockOut.bit);
    // Switched a thousand times a second, so measured and not recorded
    const levelLight = attachDutyMeter(machine.cpu, portOf(PINS.levelLight), PINS.levelLight.bit);

    // The two jacks, each through its inverting input stage, and whatever
    // is plugged into them: a clock source, and a source of reset pulses
    const clockIn = attachClockSource(
        machine.cpu, (high) => machine.inputCapture1.setPin(!high));
    const resetIn = attachClockSource(
        machine.cpu, (high) => machine.externalInt0.setPin(!high));

    // The run button closes its pin to ground
    const runButton = attachSwitch(
        machine.cpu, (closed) => portOf(PINS.runButton).setPin(PINS.runButton.bit, !closed));

    // No LFO: the modulation input sits at its centre
    machine.adc.channelValues[MOD_ADC_CHANNEL] =
        ((MOD_CENTRE_READING + 0.5) * ADC_REFERENCE_VOLTS) / ADC_STEPS;

    // Nothing plugged in and the button up: every pin high
    machine.inputCapture1.setPin(true);
    machine.externalInt0.setPin(true);
    portOf(PINS.runButton).setPin(PINS.runButton.bit, true);

    // A recorder's completed pulses, in milliseconds of chip time
    function pulsesMs(recorder) {
        return recorder.pulses().map((pulse) => ({
            riseMs: machine.cyclesToMs(pulse.rise),
            widthMs: machine.cyclesToMs(pulse.width),
        }));
    }

    // Every part sees every transfer. The step lights' chain has no chip
    // select either and takes in every byte sent, the DAC's frames too; only
    // what it holds when its latch clock rises is ever shown.
    // The 74HC165 chain has no chip select: it is
    // clocked eight times whoever the transfer is for, and what it shifts
    // out is the only thing on MISO, arriving when the transfer ends. The
    // DAC takes the byte only while it is selected. spiSent keeps the bytes
    // sent with the DAC not selected, which are the panel reads
    machine.spi.onByte = (sent) => {
        const received = chain.shiftByte();

        if (!dac.isSelected()) {
            spiSent.push(sent);
        }
        dac.shiftByte(sent);
        leds.shiftByte(sent);
        machine.cpu.addClockEvent(
            () => machine.spi.completeTransfer(received), machine.spi.transferCycles);
    };

    function takeMidiByte(byte) {
        midi.bytes.push({ value: byte, cycle: machine.cpu.cycles });
        midi.messages.push(...midiParser.push(byte, machine.cpu.cycles));
    }

    function takeLogByte(byte) {
        if (partial === '') {
            partialStart = machine.cpu.cycles;
        }
        partial += String.fromCharCode(byte);
        if (partial.endsWith('\r\n')) {
            const line = {
                text: partial.slice(0, -2),
                startCycle: partialStart,
                cycle: machine.cpu.cycles,
            };
            const list = line.text.startsWith(STEP_LINE_PREFIX) ? serial.lines : serial.notices;

            list.push(line);
            partial = '';
        }
    }

    // One wire, and the image says what is on it
    const takeByte = (WIRES[config] === WIRE_MIDI) ? takeMidiByte : takeLogByte;

    machine.usart.onByteTransmit = (byte) => {
        serial.bytes++;
        takeByte(byte);
    };

    return {
        machine,
        serial,
        midi,
        // What this image sends from the USART: a log, MIDI, or neither
        hasLog: WIRES[config] === WIRE_LOG,
        sendsMidi: WIRES[config] === WIRE_MIDI,
        panelReads: chain.reads,
        spiSent,
        // The DAC's frames: the pitch CV on channel A, the envelope on B
        cvWrites: dac.writes.A,
        envWrites: dac.writes.B,
        cvIgnoredFrames: dac.ignored,
        ledLatches: leds.latches,
        gate,
        clockOut,
        clockIn,
        resetIn,
        runButton,
        // The cycle of each reset since power-on, which only the watchdog
        // causes
        resets: machine.resets,

        /**
         * Stops the firmware's tick from outside, by switching the chip's
         * interrupts off: the loop goes on round with nothing to do and the
         * watchdog, no longer kicked, resets the chip about 256 ms later.
         * The firmware never does this to itself. Asked while the chip is
         * in its tick interrupt it does nothing, since the chip switches
         * interrupts back on as it returns; a scenario asks at a pin edge,
         * which is made in the loop.
         */
        stopTick() {
            machine.cpu.data[SREG] &= ~INTERRUPT_ENABLE;
        },

        /**
         * The crash log as the EEPROM holds it just now, read as
         * readCrashLog() in crash_record.js gives it: the slots that hold
         * a record, and which of them is the newest.
         */
        crashLog() {
            const end = CRASH_LOG_ADDRESS + CRASH_LOG_BYTES;

            return readCrashLog(machine.eepromMemory.slice(CRASH_LOG_ADDRESS, end));
        },

        /**
         * Starts a clock on the clock input jack: the first rising edge
         * now, then one every periodMs, each high for half the period.
         * With a pattern (one of CLOCK_PATTERNS) the edges are uneven and
         * periodMs is what parts/clock_patterns.js makes of it; the seed
         * picks which train a random pattern gives.
         */
        startClockIn(periodMs, pattern = 'steady', seed = undefined) {
            if (pattern === 'steady') {
                clockIn.start(machine.msToCycles(periodMs), machine.msToCycles(periodMs / 2));
            } else {
                clockIn.start(makeClockPattern(pattern, machine.msToCycles(periodMs), seed));
            }
        },

        /** Stops the clock on the clock input jack. */
        stopClockIn() {
            clockIn.stop();
        },

        /** One pulse on the clock input jack, high for widthMs (0: no time). */
        pulseClockIn(widthMs = 0) {
            clockIn.pulse(machine.msToCycles(widthMs));
        },

        /** One pulse on the reset input jack, high for widthMs (0: no time). */
        pulseResetIn(widthMs = 0) {
            resetIn.pulse(machine.msToCycles(widthMs));
        },

        /**
         * Presses the run button, holds it for holdMs of chip time and lets
         * go, the contact bouncing each way for bounceMs first (0: cleanly).
         */
        pressRunButton(holdMs, bounceMs = 0) {
            const bounces = 4;
            const flipCycles = machine.msToCycles(bounceMs / (bounces * 2));
            const end = machine.cpu.cycles + machine.msToCycles(holdMs);

            runButton.set(true, flipCycles, bounceMs > 0 ? bounces : 0);
            machine.runUntil(() => machine.cpu.cycles >= end, holdMs + 1);
            runButton.set(false, flipCycles, bounceMs > 0 ? bounces : 0);
        },

        /**
         * Puts the pattern on the external clock, waiting for an edge: a
         * stop, one clock edge while it is stopped, and a start. What was
         * recorded before the start is forgotten, so that a scenario
         * begins with a board that has played nothing.
         */
        startOnExternalClock() {
            this.runForMs(5);
            this.pressRunButton(60);
            this.pulseClockIn(2);
            this.runForMs(60);
            for (const list of [gate.edges, clockOut.edges, dac.writes.A, dac.writes.B, dac.ignored,
                chain.reads, spiSent, leds.latches, clockIn.rises, resetIn.rises,
                serial.lines, serial.notices, midi.bytes, midi.messages]) {
                list.length = 0;
            }
            this.pressRunButton(60);
        },

        /**
         * The step lights as they stood at a cycle, bit n for step n: what
         * the last latch before it showed, or 0 if there was none.
         */
        ledsAt(cycle) {
            let value = 0;
            for (const latch of leds.latches) {
                if (latch.cycle >= cycle) {
                    break;
                }
                value = latch.value;
            }
            return value;
        },

        /** Completed gate pulses: when each rose and how long it was high. */
        gatePulsesMs() {
            return pulsesMs(gate);
        },

        /** Completed clock output pulses, in the same form. */
        clockOutPulsesMs() {
            return pulsesMs(clockOut);
        },

        /**
         * The pitch CV as it stood at a cycle: the last frame the DAC took
         * before it, or null if there was none.
         */
        cvWriteBefore(cycle) {
            let latest = null;
            for (const write of dac.writes.A) {
                if (write.cycle >= cycle) {
                    break;
                }
                latest = write;
            }
            return latest;
        },

        /**
         * The envelope CV as it stood at a cycle, in millivolts: what the
         * last frame for it before then set, or 0 if there was none.
         */
        envMillivoltsAt(cycle) {
            let millivolts = 0;
            for (const write of dac.writes.B) {
                if (write.cycle >= cycle) {
                    break;
                }
                millivolts = write.millivolts;
            }
            return millivolts;
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

        /**
         * Sets the eight mode switches, as a number 0 to 255 with a bit a
         * switch. The lowest three choose the engine.
         */
        setModeSwitches(value) {
            modeSwitches = value & ((1 << MODE_BITS) - 1);
        },

        /**
         * Turns the selector to a position, 0 to 7: what the pot sets.
         * Position 0 is the tempo.
         */
        setSelector(position) {
            selector = position % SELECTOR_POSITIONS;
        },

        /** The selector's position, 0 to 7. */
        selector() {
            return selector;
        },

        /** The mode switches as they are set, 0 to 255. */
        modeSwitches() {
            return modeSwitches;
        },

        /**
         * Sets the panel as calibration mode uses it: the entry pattern on
         * the last four steps, the point to set (0 to 4) on step 0, and the
         * store switch on or off.
         */
        setCalibrationPanel(point, storeOn) {
            const firstEntryStep = STEP_COUNT - CAL_ENTRY_NOTES.length;

            notes.fill(0);
            notes[0] = point;
            CAL_ENTRY_NOTES.forEach((note, i) => {
                notes[firstEntryStep + i] = note;
            });
            if (!storeOn) {
                notes[STEP_COUNT - 1] &= ~CAL_STORE_SWITCH;
            }
        },

        /**
         * Whether the panel reads the calibration entry pattern just now. Asked
         * at power-on, it says which mode the firmware is about to choose.
         */
        readsCalibrationEntry() {
            const firstEntryStep = STEP_COUNT - CAL_ENTRY_NOTES.length;

            return CAL_ENTRY_NOTES.every((note, i) => notes[firstEntryStep + i] === note);
        },

        /** A copy of the EEPROM's bytes from address 0. */
        eepromBytes(count) {
            return Array.from(machine.eepromMemory.slice(0, count));
        },

        /**
         * How bright the level light has been since this was last asked:
         * the part of the time its pin was high, 0 to 1.
         */
        takeLevel() {
            return levelLight.take();
        },

        /** Runs for this many milliseconds of chip time. */
        runForMs(ms) {
            const end = machine.cpu.cycles + machine.msToCycles(ms);

            machine.runUntil(() => machine.cpu.cycles >= end, ms + 1);
        },

        /** Runs until this many MIDI messages have been sent whole. */
        runUntilMidiMessages(count, limitMs) {
            machine.runUntil(() => midi.messages.length >= count, limitMs);
        },

        /** Runs until this many lines other than step lines have been sent. */
        runUntilNotices(count, limitMs) {
            machine.runUntil(() => serial.notices.length >= count, limitMs);
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

        /**
         * Sets the modulation input so the firmware reads exactly this
         * value, 0 to 1023: what the LFO's input stage puts on the pin. 512
         * is an LFO at its centre, or none.
         */
        setLfoReading(reading) {
            machine.adc.channelValues[MOD_ADC_CHANNEL] =
                ((reading + 0.5) * ADC_REFERENCE_VOLTS) / ADC_STEPS;
        },

        /**
         * Sets the rate pot so the firmware reads exactly this value, 0
         * to 1023. It is at 0 on a new board, where the sequencer's own
         * LFO is off.
         */
        setRateReading(reading) {
            machine.adc.channelValues[RATE_ADC_CHANNEL] =
                ((reading + 0.5) * ADC_REFERENCE_VOLTS) / ADC_STEPS;
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

module.exports = { createBoard, CONFIGS, STEP_COUNT, CLOCK_PATTERNS };
