'use strict';
/**
 * A chain of 74HC165 parallel-in shift registers whose clock and output are
 * on an SPI bus, with the load line on an ordinary port pin.
 *
 *   SH/LD (load)  Low: the registers follow their parallel inputs. Level
 *                 sensitive, not edge triggered. High: they hold and shift.
 *   CLK           Rising edge with SH/LD high shifts one bit towards QH.
 *   QH            Output of the chip nearest the MCU. Shows the next bit
 *                 without needing a clock first.
 *
 * Clock Inhibit is taken as tied low and the far end's serial input as tied
 * low, so zeros follow the last real bit.
 *
 * The load line is modelled as a wire. The clock and QH are not: the
 * emulated chip's SPI does not toggle its pins, it hands over whole bytes.
 * So the board calls shiftByte() once per SPI transfer, and that stands for
 * eight clocks with QH sampled before each rising edge, which is what an SPI
 * master in mode 0 or 3 does. Nothing here can tell whether the firmware
 * chose a mode that samples on the right edge.
 */

const BITS_PER_BYTE = 8;

/**
 * Wires a chain's load line to a port pin.
 * @param port        The avr8js port the load line is on.
 * @param loadPin     Bit number of the load line on that port.
 * @param readInputs  Returns the parallel inputs as an array of 0/1, in the
 *                    order they leave QH. Called each time the chain loads.
 * @return { reads, shiftByte }. reads has one entry per load pulse, with the
 *         cycle it ended on and the clock pulses that followed it.
 *         shiftByte() clocks the chain eight times and returns the byte read,
 *         first bit in bit 7.
 */
function attachHc165Chain(cpu, port, loadPin, readInputs) {
    const loadMask = 1 << loadPin;
    const chain = { reads: [] };
    let bits = [];
    let loadIsLow = false;
    let pulseBegun = false;

    port.addListener((value, oldValue) => {
        const loadWasHigh = (oldValue & loadMask) !== 0;
        const loadIsHigh = (value & loadMask) !== 0;

        loadIsLow = !loadIsHigh;
        if (!loadIsHigh) {
            bits = readInputs().slice();
            pulseBegun = pulseBegun || loadWasHigh;
        } else if (!loadWasHigh) {
            // Rising edge: what the inputs held at this moment is kept. It
            // ends a load pulse only if the line was taken low from high
            // first; the first rise after power-on is the line being set to
            // its idle level, not a read
            bits = readInputs().slice();
            if (pulseBegun) {
                chain.reads.push({ cycle: cpu.cycles, clocks: 0 });
            }
            pulseBegun = false;
        } else {
            // Some other pin on the port changed
        }
    });

    chain.shiftByte = () => {
        let byte = 0;

        for (let i = 0; i < BITS_PER_BYTE; i++) {
            if (loadIsLow) {
                bits = readInputs().slice(); // Still loading: no shift
            }
            byte = (byte << 1) | (bits[0] === 1 ? 1 : 0);
            if (!loadIsLow) {
                bits.shift();
                bits.push(0);
            }
            if (chain.reads.length > 0) {
                chain.reads[chain.reads.length - 1].clocks++;
            }
        }
        return byte;
    };

    return chain;
}

module.exports = { attachHc165Chain };
