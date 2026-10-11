'use strict';
/**
 * MIDI as it goes down the wire, read back into messages. Worked out from
 * the MIDI 1.0 specification and not from the firmware, so that a scenario
 * does not check the firmware against itself.
 *
 * Only what the sequencer has any business sending is named: Start, Stop,
 * the timing clock, Note On and Note Off. Anything else comes back as
 * 'unknown', for a scenario to fail on. Running status is not accepted: the
 * firmware promises a status byte on every message, so a data byte with no
 * message open is 'unknown' as well. A real-time byte (one byte, 0xF8 and
 * up) may fall between the bytes of another message, as the specification
 * allows, and does not disturb it.
 */
const STATUS_BIT = 0x80;       // Set on a status byte, clear on a data byte
const REAL_TIME_FIRST = 0xF8;  // From here up a status is a message of its own
const KIND_MASK = 0xF0;        // A channel message's kind, and below it
const CHANNEL_MASK = 0x0F;     // its channel, 0 for channel 1
const NOTE_OFF = 0x80;
const NOTE_ON = 0x90;
const NOTE_BYTES = 2;          // Data bytes after a note's status: note, velocity
const REAL_TIME = { 0xF8: 'clock', 0xFA: 'start', 0xFC: 'stop' };
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const NOTES_PER_OCTAVE = 12;
const OCTAVE_OF_NOTE_0 = -1;   // Note 60 is C4, so note 24 is C1

/**
 * Makes a parser that is given the bytes one at a time, in the order they
 * were sent, so that a message split between two calls is still read whole.
 */
function createMidiParser() {
    let open = null; // The note message whose data bytes are still to come

    function unknown(value, cycle) {
        return { type: 'unknown', value, cycle };
    }

    return {
        /**
         * Takes the next byte and the cycle it began at. Returns the
         * messages it completed, which is none, one, or two when it cut a
         * message short. A message's cycle is that of its status byte.
         *   { type: 'start' | 'stop' | 'clock', cycle }
         *   { type: 'noteOn' | 'noteOff', channel, note, velocity, cycle }
         *     with the channel from 1 to 16
         *   { type: 'unknown', value, cycle } for any other byte
         */
        push(value, cycle) {
            const done = [];

            if (value >= REAL_TIME_FIRST) {
                const type = REAL_TIME[value];

                done.push((type === undefined) ? unknown(value, cycle) : { type, cycle });
            } else if ((value & STATUS_BIT) !== 0) {
                const kind = value & KIND_MASK;

                if (open !== null) {
                    // A new status with the last message unfinished
                    done.push(unknown(open.status, open.cycle));
                    open = null;
                }
                if ((kind === NOTE_ON) || (kind === NOTE_OFF)) {
                    open = { status: value, cycle, data: [] };
                } else {
                    done.push(unknown(value, cycle));
                }
            } else if (open === null) {
                done.push(unknown(value, cycle));
            } else {
                open.data.push(value);
                if (open.data.length === NOTE_BYTES) {
                    done.push({
                        type: ((open.status & KIND_MASK) === NOTE_ON) ? 'noteOn' : 'noteOff',
                        channel: (open.status & CHANNEL_MASK) + 1,
                        note: open.data[0],
                        velocity: open.data[1],
                        cycle: open.cycle,
                    });
                    open = null;
                }
            }
            return done;
        },

        /** Whether a message has begun and not yet had all its bytes. */
        isMidMessage() {
            return open !== null;
        },
    };
}

/**
 * Reads a whole capture, a list of { value, cycle }, into its messages. A
 * message still unfinished at the end is left out.
 */
function parseMidi(bytes) {
    const parser = createMidiParser();
    const messages = [];

    for (const byte of bytes) {
        messages.push(...parser.push(byte.value, byte.cycle));
    }
    return messages;
}

/** A note number as a name: 24 is C1, 60 is C4, 69 is A4. */
function midiNoteName(note) {
    const octave = Math.floor(note / NOTES_PER_OCTAVE) + OCTAVE_OF_NOTE_0;

    return `${NOTE_NAMES[note % NOTES_PER_OCTAVE]}${octave}`;
}

module.exports = { createMidiParser, parseMidi, midiNoteName };
