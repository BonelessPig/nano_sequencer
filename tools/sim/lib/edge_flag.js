'use strict';
/**
 * An edge flag: a bit in a flag register that the chip sets when an input
 * pin sees the chosen edge, whether or not the flag's interrupt is enabled,
 * and that the firmware clears by writing a one to it. The input capture
 * flag of a timer and the flags of the external interrupts work this way.
 *
 * avr8js does not model the input capture event at all, and sets an external
 * interrupt's flag only while that interrupt is enabled, so firmware that
 * polls a flag with its interrupt off sees nothing without this. It is
 * written from the datasheet, not checked against a chip: that the flag
 * latches with the interrupt off is the very thing the board has to confirm.
 * Nothing is modelled of the synchroniser or the noise canceller, so an edge
 * shows in the flag at once and no pulse is too short.
 */

/**
 * Puts an edge flag on an input pin.
 * @param cpu      The avr8js CPU.
 * @param port     The avr8js port the pin is on.
 * @param pin      Bit number of the pin on that port.
 * @param options  flagRegister: data address of the flag register.
 *                 flagBit: bit number of the flag in it.
 *                 edges: a function returning { rising, falling }, which of
 *                 the two the chip is set to latch just now.
 * @return { setPin, isSet }. setPin(high) drives the pin from outside, as
 *         port.setPin() does, and latches the flag if that is a chosen
 *         edge. isSet() says whether the flag is set.
 */
function attachEdgeFlag(cpu, port, pin, options) {
    const { flagRegister, flagBit, edges } = options;
    const mask = 1 << flagBit;
    const ownerHook = cpu.writeHooks[flagRegister];
    let isHigh = false;
    let latched = false;

    function show() {
        if (latched) {
            cpu.data[flagRegister] |= mask;
        } else {
            cpu.data[flagRegister] &= ~mask;
        }
    }

    // A one written to the flag clears it. The peripheral that owns the
    // rest of the register sees the write without this bit, and whatever it
    // leaves in the register, the flag is put back as the latch has it
    cpu.writeHooks[flagRegister] = (value, oldValue, address, writeMask) => {
        const others = value & ~mask;

        if (value & mask) {
            latched = false;
        }
        if (!ownerHook || !ownerHook(others, oldValue, address, writeMask)) {
            cpu.data[flagRegister] = others;
        }
        show();
        return true;
    };

    return {
        setPin(high) {
            const chosen = edges();
            const wasHigh = isHigh;

            isHigh = Boolean(high);
            port.setPin(pin, isHigh);
            if ((isHigh && !wasHigh && chosen.rising) || (!isHigh && wasHigh && chosen.falling)) {
                latched = true;
                show();
            }
        },

        isSet() {
            return latched;
        },
    };
}

module.exports = { attachEdgeFlag };
