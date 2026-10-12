'use strict';
/**
 * Which signal is on which pin of the Nano. The one place it is written down
 * for the tooling: the emulated board (nano_sequencer.js) wires its parts
 * from this, and docs/SCHEMATIC.html, which loads this file as an ordinary
 * script, labels its chip drawings from it. So the page shows the wiring that
 * `make sim` runs the firmware against.
 *
 * The firmware does not read this file. Its own account of the pins is in
 * adapters/target/, and a scenario failing is how a difference shows.
 */
const WIRING = {
    // A digital pin: its label on the Nano's header, the MCU's port and bit,
    // what it goes to, and its kind, which is its colour on the schematic.
    // via names the MCU function a pin is used through, where the board has
    // to drive that and not the port.
    pins: {
        sck: {
            nano: 'D13', port: 'B', bit: 5, kind: 'spi',
            net: 'SCK: 74HC165 CLK, 74HC595 SRCLK, DAC SCK',
        },
        miso: {
            nano: 'D12', port: 'B', bit: 4, kind: 'spi',
            net: 'MISO: QH of chip 0, through 1 kΩ',
        },
        mosi: {
            nano: 'D11', port: 'B', bit: 3, kind: 'spi',
            net: 'MOSI: DAC SDI, SER of the near 74HC595',
        },
        dacSelect: {
            nano: 'D10', port: 'B', bit: 2, kind: 'spi',
            net: 'SS: DAC chip select',
        },
        load: {
            nano: 'D9', port: 'B', bit: 1, kind: 'spi',
            net: '74HC165 SH/LD and 74HC595 RCLK',
        },
        clockIn: {
            nano: 'D8', port: 'B', bit: 0, kind: 'in', via: 'ICP1',
            net: 'ICP1: clock input, from its input stage',
        },
        runButton: {
            nano: 'D7', port: 'D', bit: 7, kind: 'sw',
            net: 'run button, to ground',
        },
        levelLight: {
            nano: 'D6', port: 'D', bit: 6, kind: 'out', via: 'OC0A',
            net: 'OC0A: level light, through a resistor',
        },
        clockOut: {
            nano: 'D5', port: 'D', bit: 5, kind: 'out',
            net: 'clock output',
        },
        gate: {
            nano: 'D4', port: 'D', bit: 4, kind: 'out',
            net: 'gate output',
        },
        resetIn: {
            nano: 'D2', port: 'D', bit: 2, kind: 'in', via: 'INT0',
            net: 'INT0: reset input, from its input stage',
        },
        transmit: {
            nano: 'TX1', port: 'D', bit: 1, kind: 'out',
            net: 'log (debug build), MIDI out (midi build)',
        },
    },

    // An analog input: its label on the header and its ADC channel
    analog: {
        pot: { nano: 'A6', channel: 6, kind: 'ana', net: 'pot wiper' },
        modulation: {
            nano: 'A7', channel: 7, kind: 'ana',
            net: 'modulation input, from its stage',
        },
    },
};

// Node takes the export; a browser has the constant above
if (typeof module !== 'undefined') {
    module.exports = WIRING;
}
