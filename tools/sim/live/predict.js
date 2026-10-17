'use strict';
/**
 * The two things the live view shows that are worked out, not read from the
 * chip: the transport and the step that is playing. Both follow the log
 * where there is one and what the server did to the controls where there is
 * not (the release and midi images); see predictRunPress(), nextStep() and
 * the server's record(). The midi image's Start and Stop say whether the
 * pattern is running, as a `Transport = N` line does, and are believed over
 * the presses; they do not name a step. The step is given as STEP_UNKNOWN
 * when it cannot be worked out: the chance engine shuffling, on an image
 * with no log. Nothing here follows the firmware's random numbers, nor the
 * amounts the pot has set: on an image with no log the step given is the one
 * the pattern has got to, which is not the one played while the LFO moves
 * the step position (selector position 4). The step lights, which are the
 * chip's own, show that one.
 *
 * MIDI in is followed for the same two things, on an image that listens to
 * it: its clock is an external clock, the sixth of them an edge as the
 * firmware counts them, and a Start makes the next step the first. Whether
 * the pattern runs is still taken from what the image sends back.
 *
 * Whether the firmware came up in calibration mode is kept here too, since
 * nothing is sequenced there and there is no transport to speak of.
 */
const { STEP_COUNT } = require('../boards/nano_sequencer');

// The low three mode switches choose the engine, as a number. Every number
// with no engine of its own plays the plain one
const ENGINE_MASK = 0x07;
const ENGINE_PLAIN = 0;
const ENGINE_STAGE = 1;
const ENGINE_KLEE = 2;
const ENGINE_CHANCE = 3;
const ENGINE_CARTESIAN = 4;
const ENGINE_EUCLID = 5;
// The stage engine: stage n's note is step n and its control step n + 8,
// whose low two switches are the stage's length in steps, less one
const STAGE_COUNT = 8;
const STAGE_LENGTH_MASK = 0x03;
// Mode switches 3 to 7 are the chosen engine's own. Those that decide which
// step is next: the Klee engine's load switch, the chance engine's shuffle
// switch, and the Cartesian engine's two settings of two switches each
const KLEE_LOAD = 0x08;
const CHANCE_SHUFFLE = 0x20;
const GRID_SIDE = 4;
const GRID_X_SHIFT = 3;
const GRID_Y_SHIFT = 5;
const GRID_SETTING_MASK = 0x03;
const STEP_UNKNOWN = -1;         // No step can be named, so none is lit
const TRANSPORT_STOPPED = 0;     // As the firmware numbers them in its log
const TRANSPORT_INTERNAL = 1;
const TRANSPORT_EXTERNAL = 2;
// MIDI in: the bytes that matter, and the clocks passed over between one
// edge and the next (six to a step, less the edge)
const MIDI_CLOCK = 0xF8;
const MIDI_START = 0xFA;
const MIDI_CONTINUE = 0xFB;
const MIDI_STOP = 0xFC;
const MIDI_CLOCKS_BETWEEN_EDGES = 5;

// The server's own, written to from here: of `outputs` the step, the stage's
// count and the transport are worked out in this file, and `panel` is where
// the controls stand. See startAfresh()
let outputs = null;
let panel = null;
let isCalibrating = false; // Powered up with the calibration entry pattern set
// An edge has come on the clock input since the last stop, which is what
// the firmware goes by to choose its clock
let hasExternalEdge = false;
let isFirstStepDue = false; // The next step to begin is the pattern's first
let stepEngine = ENGINE_PLAIN; // The engine that gave the last step counted
let midiClocksToEdge = 0;      // MIDI clocks to pass over before the next edge
// What else is kept of an engine's place, beside the last step counted: the
// chance engine's place in the order (which it keeps while it shuffles), the
// Cartesian engine's two counters as they stand for the step to come, and
// the Klee engine's load switch as it was on the last step
const place = { order: 0, x: 0, y: 0, xSteps: 0, ySteps: 0, isLoadOn: false };
// The mode switches and step switches as the chip last read them, which is
// what a step is worked out from: the firmware reads the panel every 8 ms,
// so a switch moved just before a step is not in that step. See
// takePanelRead()
const seen = { mode: 0, notes: [] };

function runningTransport() {
    return hasExternalEdge ? TRANSPORT_EXTERNAL : TRANSPORT_INTERNAL;
}

/**
 * Works everything out afresh, as the firmware starts: at power-on and after
 * a watchdog reset. It chooses its mode once, which the server tells from
 * the panel, comes up running on its own clock and plays the pattern from
 * its first step. In calibration mode nothing is sequenced and there is no
 * transport to speak of. Anything new that is predicted is put back here.
 */
function startAfresh(liveOutputs, livePanel, isInCalibration) {
    outputs = liveOutputs;
    panel = livePanel;
    isCalibrating = isInCalibration;
    hasExternalEdge = false;
    outputs.step = STEP_UNKNOWN;
    outputs.stageStep = 0;
    outputs.stageSteps = 0;
    outputs.transport = isCalibrating ? null : runningTransport();
    outputs.transportFrom = 'controls';
    isFirstStepDue = true;
    stepEngine = ENGINE_PLAIN;
    midiClocksToEdge = 0;
    seen.mode = panel.mode;
    seen.notes = panel.notes;
}

/** True if the firmware came up in calibration mode. */
function isInCalibrationMode() {
    return isCalibrating;
}

/**
 * Takes a read of the panel by the chip. The controls only change between
 * slices, so it read them as they stand now.
 */
function takePanelRead() {
    seen.mode = panel.mode;
    seen.notes = panel.notes;
}

/** The engine the mode switches choose, as the firmware takes them. */
function chosenEngine() {
    const engine = seen.mode & ENGINE_MASK;

    return (engine <= ENGINE_EUCLID) ? engine : ENGINE_PLAIN;
}

/** Counts a step towards a Cartesian counter's next move; true if it is due. */
function isGridMoveDue(counted, period) {
    place[counted]++;
    if (place[counted] < period) {
        return false;
    }
    place[counted] = 0;
    return true;
}

/**
 * The Cartesian engine: gives the step at column x of row y and moves the
 * two counters on, x every 1 to 4 steps and y every 4 to 1, as mode switches
 * 3, 4 and 5, 6 say now.
 */
function nextGridStep(isFirst) {
    const xSetting = (seen.mode >> GRID_X_SHIFT) & GRID_SETTING_MASK;
    const ySetting = (seen.mode >> GRID_Y_SHIFT) & GRID_SETTING_MASK;
    let step = 0;

    if (isFirst) {
        Object.assign(place, { x: 0, y: 0, xSteps: 0, ySteps: 0 });
    }
    step = (place.y * GRID_SIDE) + place.x;
    if (isGridMoveDue('xSteps', xSetting + 1)) {
        place.x = (place.x + 1) % GRID_SIDE;
    }
    if (isGridMoveDue('ySteps', GRID_SIDE - ySetting)) {
        place.y = (place.y + 1) % GRID_SIDE;
    }
    return step;
}

/**
 * The Klee engine: gives how many times the register has moved since it was
 * last loaded, which is what the firmware calls the step. A load is the
 * first step, and the step after mode switch 3 is moved either way. What
 * the register holds is not followed here.
 */
function nextKleePosition(isFirst) {
    const isLoadOn = (seen.mode & KLEE_LOAD) !== 0;
    const isLoad = isFirst || (isLoadOn !== place.isLoadOn);

    place.isLoadOn = isLoadOn;
    return isLoad ? 0 : (outputs.step + 1) % STEP_COUNT;
}

/**
 * The chance engine: gives the next step in order, or none that can be
 * named while mode switch 5 has it picking each step at random. The place
 * in the order is kept through that, as the firmware keeps it.
 */
function nextChanceStep(isFirst) {
    place.order = isFirst ? 0 : (place.order + 1) % STEP_COUNT;
    return ((seen.mode & CHANCE_SHUFFLE) !== 0) ? STEP_UNKNOWN : place.order;
}

/** Starts counting a stage's steps, its length as the chip last read it. */
function beginStage(stage) {
    outputs.stageStep = 1;
    outputs.stageSteps = (seen.notes[STAGE_COUNT + stage] & STAGE_LENGTH_MASK) + 1;
    return stage;
}

/**
 * Says which step the clock pulse that has just risen belongs to, as the
 * firmware's engines would have it. A pattern starts from its beginning
 * after a start or a reset made here, and when the engine chosen is not the
 * one that gave the last step. The plain engine then goes on a step a
 * pulse, and so does the Euclidean one, which only rests on some of them;
 * the stage engine stays on a stage for as many pulses as the stage has
 * steps, and names the stage, 0 to 7, each time. The others have a function
 * each, above. The chance engine's answer can be STEP_UNKNOWN.
 */
function nextStep() {
    const engine = chosenEngine();
    const isFirst = isFirstStepDue || (engine !== stepEngine);

    stepEngine = engine;
    isFirstStepDue = false;
    if (engine === ENGINE_STAGE) {
        if (!isFirst && (outputs.stageStep < outputs.stageSteps)) {
            outputs.stageStep++;
            return outputs.step;
        }
        return beginStage(isFirst ? 0 : (outputs.step + 1) % STAGE_COUNT);
    }
    outputs.stageStep = 0;
    outputs.stageSteps = 0;
    switch (engine) {
        case ENGINE_KLEE:
            return nextKleePosition(isFirst);
        case ENGINE_CHANCE:
            return nextChanceStep(isFirst);
        case ENGINE_CARTESIAN:
            return nextGridStep(isFirst);
        default:
            return isFirst ? 0 : (outputs.step + 1) % STEP_COUNT;
    }
}

/**
 * Puts the count right from a step the log has named, which was not the
 * one counted, as far as can be told: a stage is taken to be on its first
 * step, and the Cartesian counters to have only just come to that step.
 */
function followLog(step) {
    if (stepEngine === ENGINE_STAGE) {
        beginStage(step % STAGE_COUNT);
    } else if (stepEngine === ENGINE_CARTESIAN) {
        Object.assign(place, {
            x: step % GRID_SIDE, y: Math.floor(step / GRID_SIDE) % GRID_SIDE,
            xSteps: 0, ySteps: 0,
        });
        nextGridStep(false);
    } else if ((stepEngine === ENGINE_CHANCE)
        && ((seen.mode & CHANCE_SHUFFLE) === 0)) {
        place.order = step % STEP_COUNT;
    } else {
        // The others count on from the step shown
    }
}

/**
 * Takes the transport from a MIDI Start or Stop, which says whether the
 * pattern is running as surely as the log does. Which clock it runs from
 * is not in the message; that is whether an edge has come.
 */
function followMidiTransport(transport, t, events) {
    if ((outputs.transport !== transport) || (outputs.transportFrom !== 'midi')) {
        outputs.transport = transport;
        outputs.transportFrom = 'midi';
        events.push({ t, k: 'transport', v: transport, from: 'midi' });
    }
}

/**
 * Takes an edge on the clock input as the firmware does: that is the clock
 * from then until the next stop. Running, the transport changes at once;
 * stopped, the next start shows it. An image with a log says the same a
 * moment later. A stop is taken here when the button goes down and by the
 * firmware 20 ms on, so a lone edge in between is forgotten there and not
 * here until the log or the next stop puts it right.
 */
function takeExternalEdge(t, events) {
    if (!isCalibrating) {
        hasExternalEdge = true;
        if (outputs.transport === TRANSPORT_INTERNAL) {
            outputs.transport = TRANSPORT_EXTERNAL;
            outputs.transportFrom = 'controls';
            events.push({ t, k: 'transport', v: TRANSPORT_EXTERNAL, from: 'controls' });
        }
    }
}

/**
 * Takes one byte sent to MIDI in, as the firmware does: the sixth clock is
 * an edge of the external clock, the first after a Start or a Continue
 * among them; a Start or a Continue is on the MIDI clock at once and makes
 * the next step the first; a Stop forgets the external clock. Whether the
 * pattern is running is not changed here: the image's own Start or Stop
 * says so a moment later.
 */
function takeMidiIn(value, t, events) {
    if (value === MIDI_CLOCK) {
        if (midiClocksToEdge === 0) {
            midiClocksToEdge = MIDI_CLOCKS_BETWEEN_EDGES;
            takeExternalEdge(t, events);
        } else {
            midiClocksToEdge--;
        }
    } else if ((value === MIDI_START) || (value === MIDI_CONTINUE)) {
        midiClocksToEdge = 0;
        if (!isCalibrating) {
            hasExternalEdge = true;
            isFirstStepDue = true;
        }
    } else if (value === MIDI_STOP) {
        hasExternalEdge = false;
    } else {
        // A byte of a note or of anything else: the firmware passes it over
    }
}

/**
 * Says what the firmware will make of a run button press, since the button
 * only toggles and the release image never says which way it went: stopped
 * becomes running, from the first step, and running becomes stopped. It is
 * a prediction from the presses made here, each held long enough to count;
 * the debug image's `Transport = N` line, or the midi image's Start or
 * Stop, then confirms or corrects it.
 */
function predictRunPress() {
    if (isCalibrating) {
        return; // Nothing is sequenced, so the button does nothing
    }
    if (outputs.transport === TRANSPORT_STOPPED) {
        outputs.transport = runningTransport();
        isFirstStepDue = true;
    } else {
        // A stop forgets the external clock
        outputs.transport = TRANSPORT_STOPPED;
        hasExternalEdge = false;
    }
    outputs.transportFrom = 'controls';
}

/**
 * Takes a pulse on the reset input: one is taken only while running, and
 * plays the first step at the next step boundary.
 */
function predictReset() {
    if (!isCalibrating && (outputs.transport !== TRANSPORT_STOPPED)) {
        isFirstStepDue = true;
    }
}

module.exports = {
    STEP_UNKNOWN, TRANSPORT_STOPPED, startAfresh, isInCalibrationMode,
    takePanelRead, runningTransport, nextStep, followLog, followMidiTransport,
    takeExternalEdge, takeMidiIn, predictRunPress, predictReset,
};
