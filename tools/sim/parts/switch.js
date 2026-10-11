'use strict';
/**
 * A switch or push button with contacts that bounce: when it is closed or
 * opened, the line flickers between the two levels for a while before it
 * settles. The flicker is scripted on the CPU's own clock, so every run is
 * repeatable. Knows nothing about what it is wired to.
 */

/**
 * Makes a switch, open to begin with.
 * @param cpu        The avr8js CPU, whose cycle count is the time base.
 * @param setClosed  Called with true (closed) or false (open) each time the
 *                   contact changes, bounces included.
 * @return { set, isClosed }.
 */
function attachSwitch(cpu, setClosed) {
    let closed = false;

    return {
        /**
         * Closes or opens the switch. With bounceCycles and bounces given,
         * the contact first flips back and forth that many times, one flip
         * every bounceCycles, and ends where it was asked to.
         */
        set(nowClosed, bounceCycles = 0, bounces = 0) {
            closed = Boolean(nowClosed);
            setClosed(closed);
            for (let flip = 1; flip <= bounces * 2; flip++) {
                const level = (flip % 2 === 0) ? closed : !closed;

                cpu.addClockEvent(() => setClosed(level), flip * bounceCycles);
            }
        },

        /** Where the switch was last asked to be. */
        isClosed() {
            return closed;
        },
    };
}

module.exports = { attachSwitch };
