'use strict';
/**
 * A chain of 74HC165 parallel-in shift registers, as seen from the three
 * wires that reach the MCU.
 *
 *   SH/LD (load)  Low: the registers follow their parallel inputs. Level
 *                 sensitive, not edge triggered. High: they hold and shift.
 *   CLK           Rising edge with SH/LD high shifts one bit towards QH.
 *   QH            Output of the chip nearest the MCU. Shows the next bit
 *                 without needing a clock first.
 *
 * Clock Inhibit is taken as tied low and the far end's serial input as tied
 * low, so zeros follow the last real bit.
 */

/**
 * Wires a chain to one I/O port.
 * @param port        The avr8js port all three wires are on.
 * @param pins        Bit numbers on that port: { load, clock, data }.
 * @param readInputs  Returns the parallel inputs as an array of 0/1, in the
 *                    order they leave QH. Called each time the chain loads.
 * @return Counters the caller may read: { reads }, one entry per load pulse
 *         with the cycle it ended on and the clock pulses that followed it.
 */
function attachHc165Chain(cpu, port, pins, readInputs) {
    const loadMask = 1 << pins.load;
    const clockMask = 1 << pins.clock;
    const chain = { reads: [] };
    let bits = [];

    function load() {
        bits = readInputs().slice();
        port.setPin(pins.data, bits[0] === 1);
    }

    port.addListener((value, oldValue) => {
        const loadWasHigh = (oldValue & loadMask) !== 0;
        const loadIsHigh = (value & loadMask) !== 0;
        const clockRose = ((oldValue & clockMask) === 0) && ((value & clockMask) !== 0);

        if (!loadIsHigh) {
            load();
        } else if (!loadWasHigh) {
            // Rising edge: what the inputs held at this moment is kept
            load();
            chain.reads.push({ cycle: cpu.cycles, clocks: 0 });
        } else if (clockRose) {
            bits.shift();
            bits.push(0);
            port.setPin(pins.data, bits[0] === 1);
            if (chain.reads.length > 0) {
                chain.reads[chain.reads.length - 1].clocks++;
            }
        } else {
            // Some other pin on the port changed, or the clock fell
        }
    });

    return chain;
}

module.exports = { attachHc165Chain };
