'use strict';
/**
 * Records every change of level on one output pin, with the CPU cycle it
 * happened on, so a scenario can count pulses and measure their widths and
 * periods. Knows nothing about what the pin is for.
 */
const avr8js = require('avr8js');

/**
 * Starts recording a pin. The pin is taken as low until it is first driven
 * high; a pin that is still an input counts as low.
 * @param cpu   The avr8js CPU, for the cycle count.
 * @param port  The avr8js port the pin is on.
 * @param pin   Bit number of the pin on that port.
 * @return { edges, pulses, rises }. edges has one entry per change, with the
 *         cycle and the new level. pulses() pairs them up into completed
 *         high pulses; rises() gives the cycle of every rising edge.
 */
function attachPinRecorder(cpu, port, pin) {
    const recorder = { edges: [] };
    let isHigh = false;

    port.addListener(() => {
        const nowHigh = port.pinState(pin) === avr8js.PinState.High;

        if (nowHigh !== isHigh) {
            recorder.edges.push({ cycle: cpu.cycles, high: nowHigh });
            isHigh = nowHigh;
        }
    });

    /** Cycle of each rising edge so far, in order. */
    recorder.rises = () => recorder.edges
        .filter((edge) => edge.high)
        .map((edge) => edge.cycle);

    /**
     * Every high pulse that has ended, in order: the cycle it rose on and
     * how many cycles it stayed high.
     */
    recorder.pulses = () => {
        const pulses = [];
        let roseAt = null;

        for (const edge of recorder.edges) {
            if (edge.high) {
                roseAt = edge.cycle;
            } else {
                pulses.push({ rise: roseAt, width: edge.cycle - roseAt });
            }
        }
        return pulses;
    };

    return recorder;
}

module.exports = { attachPinRecorder };
