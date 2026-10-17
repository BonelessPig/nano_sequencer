'use strict';
/**
 * What the live view plugs into MIDI in: a MIDI clock at a tempo, and a
 * Start, a Stop or a Continue sent by hand. The board sends the bytes
 * (startMidiClockIn(), sendMidi()); only an image that listens takes them.
 *
 *   { midiClockOn: true | false }   The MIDI clock, running or not.
 *   { midiBpm: 30 to 300 }          Its tempo: 24 clocks a quarter note.
 *   { midiIn: 'start' | 'stop' | 'continue' }   One such message.
 *
 * Where the two controls stand is kept in the server's `panel`, as
 * midiClockOn and midiBpm, and outlasts a power cycle as the rest does.
 */
const MIN_BPM = 30;
const MAX_BPM = 300;
// A MIDI clock comes 24 times a quarter note: 60000 / 24 ms at 1 BPM
const CLOCK_MS_AT_1_BPM = 2500;
const MESSAGES = { start: 0xFA, continue: 0xFB, stop: 0xFC };

/** Starts the MIDI clock on a board if the panel has it on. */
function plugMidiIn(board, panel) {
    if (panel.midiClockOn) {
        board.startMidiClockIn(CLOCK_MS_AT_1_BPM / panel.midiBpm);
    }
}

/**
 * Applies a change from a page if it is one of MIDI in's. Returns false if
 * it is not, and then nothing was done.
 */
function applyMidiInCommand(command, board, panel) {
    if (typeof command.midiClockOn === 'boolean') {
        panel.midiClockOn = command.midiClockOn;
    } else if (Number.isInteger(command.midiBpm) && (command.midiBpm >= MIN_BPM)
        && (command.midiBpm <= MAX_BPM)) {
        panel.midiBpm = command.midiBpm;
    } else if (Object.hasOwn(MESSAGES, command.midiIn)) {
        board.sendMidi([MESSAGES[command.midiIn]]);
        return true;
    } else {
        return false;
    }
    // A running clock starts again at the tempo, with a clock at once
    board.stopMidiClockIn();
    plugMidiIn(board, panel);
    return true;
}

module.exports = { plugMidiIn, applyMidiInCommand };
