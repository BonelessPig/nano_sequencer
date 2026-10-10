'use strict';
/**
 * An MCP4822 dual 12-bit DAC with its clock and data on an SPI bus and its
 * chip select on an ordinary port pin.
 *
 *   CS    Low: the bits clocked in belong to a frame. The rising edge ends
 *         the frame. A frame of exactly 16 bits is taken; one of any other
 *         length is ignored.
 *   SDI   The frame, most significant bit first:
 *           bit 15       channel: 0 = A, 1 = B
 *           bit 14       not used
 *           bit 13       gain: 1 = 1x (2.048 V full scale), 0 = 2x (4.096 V)
 *           bit 12       1 = output on, 0 = output shut down
 *           bits 11..0   the value
 *   LDAC  Taken as tied low, so an output changes as CS rises.
 *
 * The output voltage is the value times the gain times 2.048 V / 4096, which
 * at 2x is exactly 1 mV a step. Nothing analog is modelled beyond that
 * arithmetic: no settling time, no error, no load.
 *
 * Chip select is modelled as a wire. The clock and data are not: the
 * emulated chip's SPI hands over whole bytes, so the board calls shiftByte()
 * once per SPI transfer and the part takes it only while selected. Nothing
 * here can tell which SPI mode the firmware chose; the part works in modes 0
 * and 3.
 */

const BITS_PER_BYTE = 8;
const FRAME_BITS = 16;
const CHANNEL_BIT = 0x8000;
const GAIN_1X_BIT = 0x2000;
const ACTIVE_BIT = 0x1000;
const VALUE_MASK = 0x0FFF;
const MILLIVOLTS_PER_STEP_AT_1X = 0.5; // 2.048 V over 4096 steps

/**
 * Wires a DAC's chip select to a port pin.
 * @param cpu        The avr8js CPU, for the cycle count.
 * @param port       The avr8js port chip select is on.
 * @param selectPin  Bit number of chip select on that port.
 * @return { writes, ignored, isSelected, shiftByte }. writes has one entry
 *         per frame taken: the cycle chip select rose on, the channel ('A'
 *         or 'B'), the gain (1 or 2), whether the output is on, the value
 *         and the output in millivolts. ignored has the bit count and cycle
 *         of each frame of the wrong length. shiftByte(byte) gives the part
 *         the eight bits of one SPI transfer.
 */
function attachMcp4822(cpu, port, selectPin) {
    const selectMask = 1 << selectPin;
    const dac = { writes: [], ignored: [] };
    let selected = false;
    let frame = 0;
    let bitCount = 0;

    function endFrame() {
        if (bitCount !== FRAME_BITS) {
            dac.ignored.push({ cycle: cpu.cycles, bits: bitCount });
            return;
        }
        const gain = (frame & GAIN_1X_BIT) !== 0 ? 1 : 2;
        const active = (frame & ACTIVE_BIT) !== 0;
        const value = frame & VALUE_MASK;

        dac.writes.push({
            cycle: cpu.cycles,
            channel: (frame & CHANNEL_BIT) !== 0 ? 'B' : 'A',
            gain,
            active,
            value,
            millivolts: active ? value * gain * MILLIVOLTS_PER_STEP_AT_1X : 0,
        });
    }

    port.addListener((value, oldValue) => {
        const wasHigh = (oldValue & selectMask) !== 0;
        const isHigh = (value & selectMask) !== 0;

        if (wasHigh && !isHigh) {
            selected = true;
            frame = 0;
            bitCount = 0;
        } else if (!wasHigh && isHigh && selected) {
            // The first rise after power-on is the line being set to its
            // idle level, with no frame before it
            selected = false;
            endFrame();
        } else {
            // Some other pin on the port changed
        }
    });

    dac.isSelected = () => selected;

    dac.shiftByte = (byte) => {
        if (selected) {
            // Bits beyond a frame's sixteen only need counting
            frame = ((frame << BITS_PER_BYTE) | (byte & 0xFF)) & 0xFFFF;
            bitCount += BITS_PER_BYTE;
        }
    };

    return dac;
}

module.exports = { attachMcp4822 };
