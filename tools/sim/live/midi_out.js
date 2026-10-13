'use strict';
/**
 * What the live view keeps of MIDI out. The messages are not worked out:
 * they are the board's, read from the bytes the chip sent. Three things are
 * kept of them for a page to show without a row a clock: how many clocks
 * there have been since the last Start, the tempo they are coming at, and
 * the notes a receiver would be sounding. They are `midi` in the state and
 * `now` in every `midi` event; see recordMidi().
 */
const { midiNoteName } = require('../boards/midi_messages');
const {
    TRANSPORT_STOPPED, runningTransport, followMidiTransport,
} = require('./predict');

const MIDI_CLOCKS_PER_QUARTER = 24;
// The MIDI clock's tempo is taken over the last quarter note of clocks, and
// not given until there has been a step's worth: clocks are sent on a 1 ms
// tick, so one gap alone is out by up to 6%
const MIDI_RATE_MIN_GAPS = 6;
// With no clock for this long of chip time there is no tempo to give. The
// slowest the firmware sends them, at 30 BPM, is one every 83 ms
const MIDI_CLOCK_QUIET_MS = 1000;
const MS_PER_MINUTE = 60000;

// What is kept of MIDI out, from its messages: the clocks since the last
// Start, the tempo they are coming at (null when it cannot be told), the
// notes given a Note On and no Note Off yet, and the chip time of each of
// the last 25 clocks, which the tempo is taken from. See recordMidi()
const midiOut = { clocks: 0, bpm: null, sounding: [], clockTimes: [] };
let midiClockMs = null;    // Chip time of the last clock in this slice, if any

/**
 * What is kept of MIDI out, as the state and every `midi` event carry it:
 * the clocks since the last Start, their tempo or null, and each note
 * sounding with its name.
 */
function midiSummary() {
    return {
        clocks: midiOut.clocks,
        bpm: midiOut.bpm,
        sounding: midiOut.sounding.map((note) => ({ note, name: midiNoteName(note) })),
    };
}

/** Counts a MIDI clock, and takes the tempo over the last quarter note. */
function countMidiClock(t) {
    const times = midiOut.clockTimes;

    midiOut.clocks++;
    midiClockMs = t;
    times.push(t);
    if (times.length > (MIDI_CLOCKS_PER_QUARTER + 1)) {
        times.shift();
    }
    if (times.length > MIDI_RATE_MIN_GAPS) {
        const quarterMs = ((t - times[0]) * MIDI_CLOCKS_PER_QUARTER) / (times.length - 1);

        midiOut.bpm = MS_PER_MINUTE / quarterMs;
    }
}

/**
 * Gives the tempo up when the clocks have stopped coming: stopped after an
 * external edge, or following an external clock that has gone away.
 */
function addMidiClockQuiet(nowMs, events) {
    const times = midiOut.clockTimes;

    if ((times.length > 0) && ((nowMs - times[times.length - 1]) > MIDI_CLOCK_QUIET_MS)) {
        midiOut.bpm = null;
        midiOut.clockTimes = [];
        events.push({ t: nowMs, k: 'midi', v: 'quiet', now: midiSummary() });
    }
}

/** Forgets what was kept, as a power cycle does. */
function forgetMidi() {
    Object.assign(midiOut, { clocks: 0, bpm: null, sounding: [], clockTimes: [] });
}

/** Begins a slice with no clock counted in it yet. */
function beginMidiSlice() {
    midiClockMs = null;
}

/** Adds one event for the slice's MIDI clocks, however many, if any came. */
function addMidiClocks(events) {
    if (midiClockMs !== null) {
        events.push({ t: midiClockMs, k: 'midi', v: 'clock', now: midiSummary() });
    }
}

/**
 * Takes one MIDI message the midi image sent. A clock is only counted:
 * addMidiClocks() adds one event for all of a slice's. Anything else is an event of
 * its own. A Start begins the count of clocks again, as a receiver would
 * count its bars from it, and the notes sounding are kept as a receiver
 * would have them: a Note On adds one, unless it is sounding already, and
 * a Note Off takes it away.
 */
function recordMidi(message, t, events) {
    const event = { t, k: 'midi', v: message.type };

    switch (message.type) {
        case 'clock':
            countMidiClock(t);
            return;
        case 'start':
            Object.assign(midiOut, { clocks: 0, bpm: null, clockTimes: [] });
            followMidiTransport(runningTransport(), t, events);
            break;
        case 'stop':
            followMidiTransport(TRANSPORT_STOPPED, t, events);
            break;
        case 'noteOn':
            if (!midiOut.sounding.includes(message.note)) {
                midiOut.sounding.push(message.note);
            }
            break;
        case 'noteOff':
            midiOut.sounding = midiOut.sounding.filter((note) => note !== message.note);
            break;
        default:
            // A byte that is none of the messages the firmware should send
            event.byte = message.value;
            break;
    }
    if (message.note !== undefined) {
        event.note = message.note;
        event.name = midiNoteName(message.note);
        event.vel = message.velocity;
        event.ch = message.channel;
    }
    event.now = midiSummary();
    events.push(event);
}

module.exports = {
    midiSummary, recordMidi, forgetMidi, beginMidiSlice, addMidiClocks,
    addMidiClockQuiet,
};
