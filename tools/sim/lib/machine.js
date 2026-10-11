'use strict';
/**
 * An emulated ATmega328P built from avr8js: the CPU plus the peripherals the
 * firmware uses. Knows nothing about what is wired to the chip; boards/ adds
 * that. Time is counted in CPU clock cycles, so every run is repeatable.
 */
const fs = require('node:fs');
const avr8js = require('avr8js');
const { attachEdgeFlag } = require('./edge_flag');

const FLASH_BYTES = 0x8000; // 32 KB of program memory
const EEPROM_BYTES = 1024;
const HEX_DATA_RECORD = 0;  // Intel HEX record type that carries data
const MS_PER_SECOND = 1000;
// Two resets no further apart than this are one: no program gets anywhere
// in so few cycles, and no watchdog times out in them
const SAME_RESET_CYCLES = 16;

// The two edge flags the firmware polls, by data address and bit
const TIFR1 = 0x36;      // Timer/Counter1 flags; bit 5 is the input capture flag
const ICF1 = 5;
const TCCR1B = 0x81;     // Bit 6 chooses the capture edge: set for rising
const ICES1 = 6;
const ICP1_PIN = 0;      // On port B
const EIFR = 0x3c;       // External interrupt flags; bit 0 is INT0's
const INTF0 = 0;
const EICRA = 0x69;      // Bits 1 and 0 choose INT0's edge
const INT0_PIN = 2;      // On port D
const SENSE_MASK = 0x3;
const SENSE_ANY_EDGE = 1;
const SENSE_FALLING = 2;
const SENSE_RISING = 3;

/**
 * Reads an Intel HEX file into a flash image. Only data records are used,
 * which is all a 32 KB image needs.
 */
function loadHex(hexPath) {
    const flash = new Uint8Array(FLASH_BYTES);

    for (const line of fs.readFileSync(hexPath, 'utf8').split(/\r?\n/)) {
        if (!line.startsWith(':')) {
            continue;
        }
        const count = parseInt(line.slice(1, 3), 16);
        const address = parseInt(line.slice(3, 7), 16);
        const type = parseInt(line.slice(7, 9), 16);
        if (type !== HEX_DATA_RECORD) {
            continue;
        }
        for (let i = 0; i < count; i++) {
            const at = 9 + (i * 2);
            flash[address + i] = parseInt(line.slice(at, at + 2), 16);
        }
    }

    return flash;
}

/**
 * Creates the chip with the given image loaded, stopped at reset.
 * @param hexPath  Intel HEX file to run.
 * @param clockHz  CPU clock in Hz; must match the F_CPU the image was built for.
 */
function createAtmega328p(hexPath, clockHz) {
    const flash = loadHex(hexPath);
    const cpu = new avr8js.CPU(new Uint16Array(flash.buffer));
    const eepromBackend = new avr8js.EEPROMMemoryBackend(EEPROM_BYTES);
    const machine = {
        cpu,
        clockHz,
        eeprom: new avr8js.AVREEPROM(cpu, eepromBackend),
        // The EEPROM's contents, erased (0xFF) to begin with. Write to it
        // before running to start with something stored
        eepromMemory: eepromBackend.memory,
        portB: new avr8js.AVRIOPort(cpu, avr8js.portBConfig),
        portC: new avr8js.AVRIOPort(cpu, avr8js.portCConfig),
        portD: new avr8js.AVRIOPort(cpu, avr8js.portDConfig),
        adc: new avr8js.AVRADC(cpu, avr8js.adcConfig),
        usart: new avr8js.AVRUSART(cpu, avr8js.usart0Config, clockHz),
        timer0: new avr8js.AVRTimer(cpu, avr8js.timer0Config),
        timer1: new avr8js.AVRTimer(cpu, avr8js.timer1Config),
        timer2: new avr8js.AVRTimer(cpu, avr8js.timer2Config),
        spi: new avr8js.AVRSPI(cpu, avr8js.spiConfig, clockHz),
        // On timing out it restarts the program and sets the watchdog reset
        // flag; the rest of the chip is left as it was, which a real reset
        // would not do
        watchdog: new avr8js.AVRWatchdog(
            cpu, avr8js.watchdogConfig, new avr8js.AVRClock(cpu, clockHz)),
        // The cycle of each reset since the chip was created. Whoever reads
        // the list may empty it
        resets: [],

        /** Converts a cycle count to milliseconds of chip time. */
        cyclesToMs(cycles) {
            return (cycles * MS_PER_SECOND) / clockHz;
        },

        /** Converts milliseconds of chip time to a cycle count. */
        msToCycles(ms) {
            return (ms * clockHz) / MS_PER_SECOND;
        },

        /**
         * Runs until isDone() returns true. Throws if that takes longer than
         * limitMs of chip time, so a scenario can never hang.
         */
        runUntil(isDone, limitMs) {
            const limit = cpu.cycles + machine.msToCycles(limitMs);

            while (!isDone()) {
                if (cpu.cycles >= limit) {
                    throw new Error(`not reached within ${limitMs} ms of chip time`);
                }
                avr8js.avrInstruction(cpu);
                cpu.tick();
            }
        },
    };

    // The emulator's reset, which only its watchdog ever asks for, restarts
    // the program and leaves every register as it was. A real reset puts
    // them back, and one of them matters to the firmware: the USART's
    // "ready for a byte" flag, without which nothing is ever sent again
    const restartProgram = cpu.reset.bind(cpu);
    let lastReset = null; // The cycle of the last one, or null
    cpu.reset = () => {
        restartProgram();
        machine.usart.reset();
        // The emulator's watchdog asks again an instruction later when its
        // control register has been written twice since the last reset,
        // as the timed sequence that sets it does. That is one reset
        if ((lastReset === null) || ((cpu.cycles - lastReset) > SAME_RESET_CYCLES)) {
            machine.resets.push(cpu.cycles);
        }
        lastReset = cpu.cycles;
    };

    // Drive these two pins through their setPin(), not the port's, so that
    // an edge is latched in its flag (see edge_flag.js)
    machine.inputCapture1 = attachEdgeFlag(cpu, machine.portB, ICP1_PIN, {
        flagRegister: TIFR1,
        flagBit: ICF1,
        edges() {
            const rising = (cpu.data[TCCR1B] & (1 << ICES1)) !== 0;

            return { rising, falling: !rising };
        },
    });
    machine.externalInt0 = attachEdgeFlag(cpu, machine.portD, INT0_PIN, {
        flagRegister: EIFR,
        flagBit: INTF0,
        edges() {
            const sense = cpu.data[EICRA] & SENSE_MASK;

            // A sense of 0 is the low level, which latches nothing
            return {
                rising: sense === SENSE_RISING || sense === SENSE_ANY_EDGE,
                falling: sense === SENSE_FALLING || sense === SENSE_ANY_EDGE,
            };
        },
    });

    return machine;
}

module.exports = { createAtmega328p };
