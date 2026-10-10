'use strict';
/**
 * An emulated ATmega328P built from avr8js: the CPU plus the peripherals the
 * firmware uses. Knows nothing about what is wired to the chip; boards/ adds
 * that. Time is counted in CPU clock cycles, so every run is repeatable.
 */
const fs = require('node:fs');
const avr8js = require('avr8js');

const FLASH_BYTES = 0x8000; // 32 KB of program memory
const HEX_DATA_RECORD = 0;  // Intel HEX record type that carries data
const MS_PER_SECOND = 1000;

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
    const machine = {
        cpu,
        clockHz,
        portB: new avr8js.AVRIOPort(cpu, avr8js.portBConfig),
        portC: new avr8js.AVRIOPort(cpu, avr8js.portCConfig),
        portD: new avr8js.AVRIOPort(cpu, avr8js.portDConfig),
        adc: new avr8js.AVRADC(cpu, avr8js.adcConfig),
        usart: new avr8js.AVRUSART(cpu, avr8js.usart0Config, clockHz),
        timer2: new avr8js.AVRTimer(cpu, avr8js.timer2Config),

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

    return machine;
}

module.exports = { createAtmega328p };
