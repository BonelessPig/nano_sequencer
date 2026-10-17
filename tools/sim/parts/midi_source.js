'use strict';
/**
 * A MIDI source: something outside the chip that sends MIDI bytes down a
 * serial line, such as a keyboard or a computer. It sends one byte after
 * another on the CPU's own clock, so every run is repeatable, and can send
 * the MIDI clock by itself. Knows nothing about what it is plugged into.
 */

const MIDI_CLOCK = 0xF8;

/**
 * Makes a MIDI source.
 * @param cpu         The avr8js CPU, whose cycle count is the time base.
 * @param deliver     Called with each byte as its last bit arrives.
 * @param byteCycles  How long a byte takes on the line: ten bits at 31250
 *                    baud is 320 us.
 * @return { send, startClock, stopClock, sent }. sent lists each byte
 *         delivered as { value, cycle }, the cycle the one it arrived in.
 */
function attachMidiSource(cpu, deliver, byteCycles) {
    const source = { sent: [] };
    const waiting = [];    // Bytes still to go, the next one first
    let isSending = false; // A byte is on the line
    let running = null;    // The clock in progress, or null

    function sendNext() {
        if (waiting.length === 0) {
            isSending = false;
            return;
        }
        const value = waiting.shift();

        isSending = true;
        cpu.addClockEvent(() => {
            source.sent.push({ value, cycle: cpu.cycles });
            deliver(value);
            sendNext();
        }, byteCycles);
    }

    /**
     * Sends bytes, in order, behind any still waiting: the first begins
     * now if the line is free, and each arrives a byte's time after the
     * one before.
     */
    source.send = (bytes) => {
        waiting.push(...bytes);
        if (!isSending) {
            sendNext();
        }
    };

    /**
     * Starts the MIDI clock: one clock byte now and one every periodCycles.
     * Replaces a clock in progress.
     */
    source.startClock = (periodCycles) => {
        const clock = {};

        function tick() {
            if (running !== clock) {
                return;
            }
            source.send([MIDI_CLOCK]);
            cpu.addClockEvent(tick, periodCycles);
        }

        running = clock;
        tick();
    };

    /** Stops the clock. A clock byte already on the line still arrives. */
    source.stopClock = () => {
        running = null;
    };

    return source;
}

module.exports = { attachMidiSource };
