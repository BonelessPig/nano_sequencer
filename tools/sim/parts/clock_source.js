'use strict';
/**
 * A clock source: something outside the chip that puts out a pulse train,
 * such as another sequencer or an LFO. It drives one line high and low on
 * the CPU's own clock, so every run is repeatable. Knows nothing about what
 * it is plugged into.
 */

/**
 * Makes a clock source.
 * @param cpu       The avr8js CPU, whose cycle count is the time base.
 * @param setLevel  Called with true or false each time the output changes.
 * @return { start, stop, pulse, rises }. rises lists the cycle of every
 *         rising edge it has put out.
 */
function attachClockSource(cpu, setLevel) {
    const source = { rises: [] };
    let running = null; // The pulse train in progress, or null

    function rise() {
        source.rises.push(cpu.cycles);
        setLevel(true);
    }

    /**
     * Starts a pulse train: the first rising edge now, then one every
     * periodCycles, each high for highCycles. Replaces a train in progress.
     * For an uneven train, give a function in place of periodCycles: it is
     * asked as each pulse rises and answers { gap, high }, the cycles to
     * the next rising edge and the cycles this pulse is high.
     */
    source.start = (periodCycles, highCycles) => {
        const train = (typeof periodCycles === 'function')
            ? { nextPulse: periodCycles }
            : { nextPulse: () => ({ gap: periodCycles, high: highCycles }) };

        function tick() {
            if (running !== train) {
                return;
            }
            const pulse = train.nextPulse();

            rise();
            cpu.addClockEvent(() => setLevel(false), pulse.high);
            cpu.addClockEvent(tick, pulse.gap);
        }

        running = train;
        tick();
    };

    /** Stops the train. A pulse that is high still falls when it was due. */
    source.stop = () => {
        running = null;
    };

    /** One pulse now, high for highCycles, or as short as can be if 0. */
    source.pulse = (highCycles) => {
        rise();
        if (highCycles > 0) {
            cpu.addClockEvent(() => setLevel(false), highCycles);
        } else {
            setLevel(false);
        }
    };

    return source;
}

module.exports = { attachClockSource };
