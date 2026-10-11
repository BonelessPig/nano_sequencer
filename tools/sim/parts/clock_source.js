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
     */
    source.start = (periodCycles, highCycles) => {
        const train = { periodCycles, highCycles };

        function tick() {
            if (running !== train) {
                return;
            }
            rise();
            cpu.addClockEvent(() => setLevel(false), train.highCycles);
            cpu.addClockEvent(tick, train.periodCycles);
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
