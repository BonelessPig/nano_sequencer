'use strict';
/**
 * A chain of 74HC595 serial-in shift registers with output latches, whose
 * clock and data are on an SPI bus and whose latch clock is on an ordinary
 * port pin.
 *
 *   SER, SRCLK    Each rising clock edge shifts one bit in, towards QH and
 *                 on into the next chip.
 *   RCLK (latch)  Rising edge: the outputs take what the shift registers
 *                 hold. Edge triggered; nothing shows before it.
 *   QA to QH      The latched outputs. QA holds the bit shifted in last.
 *
 * Output Enable is taken as tied low and the shift register clear as tied
 * high, so the outputs always drive and nothing clears the chain.
 *
 * The latch line is modelled as a wire. The clock and data are not: the
 * emulated chip's SPI hands over whole bytes, so the board calls shiftByte()
 * once per SPI transfer, most significant bit first. There is no chip
 * select: every byte sent on the bus goes through the chain.
 */

const BITS_PER_BYTE = 8;

/**
 * Wires a chain's latch clock to a port pin.
 * @param port       The avr8js port the latch clock is on.
 * @param latchPin   Bit number of the latch clock on that port.
 * @param chipCount  Chips in the chain, up to 4.
 * @return { latches, outputs, shiftByte }. latches has one entry per rising
 *         edge of the latch clock, with its cycle and the value the outputs
 *         took: bit 0 is QA of the chip nearest the MCU, bit 8 QA of the
 *         next. outputs() gives the value now. shiftByte(byte) clocks eight
 *         bits in.
 */
function attachHc595Chain(cpu, port, latchPin, chipCount) {
    const latchMask = 1 << latchPin;
    const bits = chipCount * BITS_PER_BYTE;
    const chain = { latches: [] };
    let shifted = 0;
    let latched = 0;

    port.addListener((value, oldValue) => {
        const wasHigh = (oldValue & latchMask) !== 0;
        const isHigh = (value & latchMask) !== 0;

        if (!wasHigh && isHigh) {
            latched = shifted;
            chain.latches.push({ cycle: cpu.cycles, value: latched });
        }
    });

    chain.outputs = () => latched;

    chain.shiftByte = (byte) => {
        // Kept within the chain's length; what falls off the far end is lost
        shifted = ((shifted * (1 << BITS_PER_BYTE)) + (byte & 0xFF)) % (2 ** bits);
    };

    return chain;
}

module.exports = { attachHc595Chain };
