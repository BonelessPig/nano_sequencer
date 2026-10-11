'use strict';
/**
 * Measures how much of the time one output pin is high, for a pin that is
 * switched too fast to be worth recording edge by edge (a pulse width
 * modulated light). Knows nothing about what the pin is for.
 */
const avr8js = require('avr8js');

/**
 * Starts measuring a pin. A pin that is still an input counts as low.
 * @param cpu   The avr8js CPU, for the cycle count.
 * @param port  The avr8js port the pin is on.
 * @param pin   Bit number of the pin on that port.
 * @return { take }. take() gives the part of the time the pin was high
 *         since it was last called, or since the meter was attached, as a
 *         number from 0 to 1, and starts measuring afresh. It gives 0 if no
 *         time has passed.
 */
function attachDutyMeter(cpu, port, pin) {
    let isHigh = false;
    let since = cpu.cycles;      // When the level last changed, or the last take
    let highCycles = 0;          // Cycles spent high since the last take
    let startCycle = cpu.cycles; // When the last take was

    function settle() {
        if (isHigh) {
            highCycles += cpu.cycles - since;
        }
        since = cpu.cycles;
    }

    port.addListener(() => {
        const nowHigh = port.pinState(pin) === avr8js.PinState.High;

        if (nowHigh !== isHigh) {
            settle();
            isHigh = nowHigh;
        }
    });

    return {
        take() {
            settle();
            const total = cpu.cycles - startCycle;
            const duty = (total > 0) ? (highCycles / total) : 0;

            highCycles = 0;
            startCycle = cpu.cycles;
            return duty;
        },
    };
}

module.exports = { attachDutyMeter };
