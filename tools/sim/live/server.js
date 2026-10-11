'use strict';
/**
 * The emulated board, live (`make live`): runs one firmware image at the pace
 * of a real chip and serves a page that shows what the board does and lets
 * its panel be worked. The chip and the parts are the same ones the scenarios
 * use; only this file and page.html know about a browser.
 *
 *   GET  /         The page.
 *   GET  /events   A stream of server-sent events: `state` (the panel, which
 *                  image is running, its mode, the output levels and the
 *                  step lights) when a page
 *                  connects and when a control changes, and `run` every
 *                  slice with the chip time and what happened in it.
 *   POST /control  One change, as JSON:
 *                  { notes: [16 values, 0 to 15] }
 *                  { mode: 0 to 255 }           The eight mode switches, a
 *                                               bit a switch.
 *                  { tempo: 0 to 1023 }
 *                  { sync: true | false }       The sync switch.
 *                  { run: 'press' }             One press of the run button.
 *                  { pulse: 'clock' | 'reset' } One pulse on that input jack.
 *                  { clockOn: true | false }    The clock source plugged
 *                                               into the clock input jack.
 *                  { clockMs: 20 to 1000 }      Its period.
 *                  { clockPattern: name }       How even its edges are:
 *                                               one of the board's
 *                                               CLOCK_PATTERNS.
 *                  { tick: 'stop' }             Stops the firmware's tick,
 *                                               for the watchdog to catch.
 *                  { power: 'debug' | 'release', keepEeprom: true | false },
 *                  which is a power cycle.
 *
 * Unlike a scenario this is paced by the PC's clock, so no two runs are the
 * same; nothing is asserted here. The chip stops while no page is connected.
 *
 * Two things shown are worked out here, not read from the chip: the
 * transport and the step that is playing. Both follow the log where there is
 * one and what this server did to the controls where there is not; see
 * predictRunPress(), nextStep() and record(). The step is sent as -1 when
 * it cannot be worked out: the chance engine shuffling, on an image with no
 * log. Nothing here follows the firmware's random numbers.
 *
 * The sixteen step lights are not worked out: `leds` in the state and the
 * `led` events are what the two 74HC595s last latched, bit n for step n.
 *
 * Nor is the crash log: `crashLog` in the state and the `crash` events are
 * what the board reads from the EEPROM, looked at after every slice. A
 * watchdog reset is taken from the chip, not from the log, so it is seen on
 * the release image too: it is a `watchdog` event, and the transport, the
 * step and the mode are then worked out afresh, as at power-on.
 */
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { createBoard, STEP_COUNT, CLOCK_PATTERNS } = require('../boards/nano_sequencer');

const HOST = '127.0.0.1'; // This PC only
const DEFAULT_PORT = 8165;
const PAGE_PATH = path.join(__dirname, 'page.html');
const CONFIGS = ['debug', 'release'];
const SLICE_MS = 10;         // How often the chip is run on and the page told
const MAX_SLICE_CHIP_MS = 50; // The most chip time one slice may catch up
const SPEED_WINDOW_MS = 1000;
const EEPROM_BYTES = 1024;
const POWER_ON_RESET = 0x01;
const MAX_NOTE = 15;
const MAX_MODE = 255;
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
const MAX_TEMPO_READING = 1023;
const MAX_BODY_BYTES = 1024;
const START_NOTES = [1, 8, 13, 8, 4, 8, 11, 0, 1, 8, 13, 8, 6, 9, 13, 0];
const START_TEMPO_READING = 480; // 150 BPM
const START_CLOCK_MS = 100;      // A step to an edge at this is 150 BPM too
const MIN_CLOCK_MS = 20;
const MAX_CLOCK_MS = 1000;
const JACK_PULSE_MS = 5;         // How long a single pulse on a jack is high
// A press of the run button holds it down this long, and the next press is
// not taken until it has been up as long: both well over the 20 ms the
// firmware wants before it believes the button, so every press counts once
const RUN_HOLD_MS = 60;
const TRANSPORT_STOPPED = 0;     // As the firmware numbers them in its log
const TRANSPORT_INTERNAL = 1;
const TRANSPORT_EXTERNAL = 2;
const TRANSPORT_LINE = /^Transport = ([0-2])$/;
const STEP_LINE = /^Step (\d+) /;

// Where the controls stand. They outlast a power cycle, as the real ones do
const panel = {
    config: CONFIGS[0],
    notes: START_NOTES.slice(),
    mode: 0,
    tempo: START_TEMPO_READING,
    sync: false,
    clockOn: false,
    clockMs: START_CLOCK_MS,
    clockPattern: CLOCK_PATTERNS[0], // Steady
};
const clients = new Set();
let board = null;
let outputs = null;
let isCalibrating = false; // Powered up with the calibration entry pattern set
let isFirstStepDue = false; // The next step to begin is the pattern's first
let stepEngine = ENGINE_PLAIN; // The engine that gave the last step counted
// What else is kept of an engine's place, beside the last step counted: the
// chance engine's place in the order (which it keeps while it shuffles), the
// Cartesian engine's two counters as they stand for the step to come, and
// the Klee engine's load switch as it was on the last step
const place = { order: 0, x: 0, y: 0, xSteps: 0, ySteps: 0, isLoadOn: false };
// The mode switches and step switches as the chip last read them, which is
// what a step is worked out from: the firmware reads the panel every 8 ms,
// so a switch moved just before a step is not in that step. See record()
const seen = { mode: 0, notes: START_NOTES };
let runUpDueMs = null;     // Chip time at which the run button is let go
let runFreeMs = 0;         // Chip time from which it may be pressed again
let isTickStopped = false; // The tick was stopped here and no reset has come
let watchdogResets = 0;    // How many there have been since power-up
let crashLog = null;       // The crash log as the board last read it
let timer = null;
let wallStartMs = 0;  // The wall time at which the chip's time was zero
let speed = { wallMs: 0, chipMs: 0, percent: 100 };

function chipMs() {
    return board.machine.cyclesToMs(board.machine.cpu.cycles);
}

function runningTransport() {
    return panel.sync ? TRANSPORT_EXTERNAL : TRANSPORT_INTERNAL;
}

/** Starts a board from reset with the panel as it stands. */
function powerUp(config, eeprom) {
    board = createBoard(config, { eeprom, resetFlags: POWER_ON_RESET });
    board.setNotes(panel.notes);
    board.setModeSwitches(panel.mode);
    board.setTempoReading(panel.tempo);
    // A toggle switch stays where it was and whatever is plugged into the
    // clock input carries on; the run button is up
    board.setSyncSwitch(panel.sync);
    if (panel.clockOn) {
        board.startClockIn(panel.clockMs, panel.clockPattern);
    }
    panel.config = config;
    // The firmware chooses its mode once, from the panel as it is now. This
    // is worked out from the panel, not read from the chip
    isCalibrating = board.readsCalibrationEntry();
    // The firmware comes up running. In calibration mode nothing is
    // sequenced and there is no transport to speak of
    outputs = {
        gate: false,
        clock: false,
        step: -1,
        stageStep: 0,   // The stage engine: which step of its stage, from 1,
        stageSteps: 0,  // and how many steps the stage lasts
        millivolts: 0,
        leds: 0,        // The step lights as last latched, bit n for step n
        transport: isCalibrating ? null : runningTransport(),
        transportFrom: 'controls',
    };
    isFirstStepDue = true;
    stepEngine = ENGINE_PLAIN;
    seen.mode = panel.mode;
    seen.notes = panel.notes;
    runUpDueMs = null;
    runFreeMs = 0;
    isTickStopped = false;
    watchdogResets = 0;
    crashLog = board.crashLog();
    wallStartMs = performance.now();
    speed = { wallMs: wallStartMs, chipMs: 0, percent: 100 };
}

/**
 * Takes a watchdog reset. The firmware starts again as it does at power-on:
 * it chooses its mode from the panel as it stands, comes up running and
 * plays the pattern from its first step. The pins are left as the events
 * give them, and the run button where it is: one held down through a
 * start-up is not a press.
 */
function startAgain() {
    isCalibrating = board.readsCalibrationEntry();
    outputs.step = -1;
    outputs.stageStep = 0;
    outputs.stageSteps = 0;
    outputs.transport = isCalibrating ? null : runningTransport();
    outputs.transportFrom = 'controls';
    isFirstStepDue = true;
    stepEngine = ENGINE_PLAIN;
    seen.mode = panel.mode;
    seen.notes = panel.notes;
    isTickStopped = false;
    watchdogResets++;
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
 * Takes one thing the board recorded: brings `outputs` up to date with it
 * and adds what the page should be told to `events`.
 *
 * The step playing is counted from the clock output, which pulses once a
 * step; see nextStep(). The debug image also names each step in its log,
 * just after the pulse, and that is believed over the count. A step sent
 * as STEP_UNKNOWN is one the count cannot name, which only the log can.
 */
function record(item, events) {
    const t = board.machine.cyclesToMs(item.cycle);
    const setStep = (step) => {
        outputs.step = (step === STEP_UNKNOWN) ? step : step % STEP_COUNT;
        events.push({
            t, k: 'step', v: outputs.step,
            n: outputs.stageStep, of: outputs.stageSteps,
        });
    };

    switch (item.kind) {
        case 'gate':
            outputs.gate = item.value;
            events.push({ t, k: 'gate', v: item.value });
            break;
        case 'clock':
            outputs.clock = item.value;
            events.push({ t, k: 'clock', v: item.value });
            if (item.value) {
                setStep(nextStep());
            }
            break;
        case 'cv':
            // The DAC is written every tick; only a change is worth telling
            if (item.value !== outputs.millivolts) {
                outputs.millivolts = item.value;
                events.push({ t, k: 'cv', v: item.value });
            }
            break;
        case 'read':
            // The chip has read the panel. The controls only change between
            // slices, so it read them as they stand now
            seen.mode = panel.mode;
            seen.notes = panel.notes;
            break;
        case 'led':
            // The lights are latched at every panel read, 8 ms apart
            if (item.value !== outputs.leds) {
                outputs.leds = item.value;
                events.push({ t, k: 'led', v: item.value });
            }
            break;
        case 'log': {
            const transport = TRANSPORT_LINE.exec(item.value);
            const step = STEP_LINE.exec(item.value);

            events.push({ t, k: 'log', v: item.value });
            if (transport !== null) {
                outputs.transport = Number(transport[1]);
                outputs.transportFrom = 'log';
                events.push({ t, k: 'transport', v: outputs.transport, from: 'log' });
            }
            if ((step !== null) && (Number(step[1]) !== outputs.step)) {
                // The count was out, or could not name the step
                followLog(Number(step[1]));
                setStep(Number(step[1]));
            }
            break;
        }
        case 'watchdog':
            // The controls only change between slices, so the firmware
            // started again with the panel as it stands now
            startAgain();
            events.push({ t, k: 'watchdog' });
            break;
        default:
            // An edge on the clock input or the reset input
            events.push({ t, k: item.kind });
            break;
    }
}

/**
 * Takes everything the board has recorded since the last call, in the order
 * it happened, and empties the records so a long run does not fill memory.
 */
function drain() {
    const { serial } = board;
    const items = [];
    const events = [];
    const add = (cycle, kind, value) => items.push({ cycle, kind, value });

    board.gate.edges.forEach((edge) => add(edge.cycle, 'gate', edge.high));
    board.clockOut.edges.forEach((edge) => add(edge.cycle, 'clock', edge.high));
    board.cvWrites.forEach((write) => add(write.cycle, 'cv', write.millivolts));
    board.ledLatches.forEach((latch) => add(latch.cycle, 'led', latch.value));
    board.panelReads.forEach((read) => add(read.cycle, 'read'));
    board.clockIn.rises.forEach((cycle) => add(cycle, 'clockin'));
    board.resetIn.rises.forEach((cycle) => add(cycle, 'resetin'));
    board.resets.forEach((cycle) => add(cycle, 'watchdog'));
    for (const line of [...serial.lines, ...serial.notices]) {
        add(line.startCycle, 'log', line.text);
    }
    // A step's clock pulse comes ahead of its log line, and stays ahead
    items.sort((a, b) => a.cycle - b.cycle);
    items.forEach((item) => record(item, events));

    for (const list of [board.gate.edges, board.clockOut.edges, board.cvWrites,
        board.cvIgnoredFrames, board.panelReads, board.spiSent,
        board.ledLatches, board.clockIn.rises, board.resetIn.rises,
        board.resets, serial.lines, serial.notices]) {
        list.length = 0;
    }
    return events;
}

function send(client, event, data) {
    client.write(`event: ${event}\ndata: ${JSON.stringify(data)}\n\n`);
}

function broadcast(event, data) {
    for (const client of clients) {
        send(client, event, data);
    }
}

/** What a page needs to draw itself. isFresh: forget what was drawn before. */
function stateMessage(isFresh) {
    return {
        ...panel, ...outputs, runDown: board.runButton.isClosed(),
        calibrating: isCalibrating, ms: chipMs(), fresh: isFresh,
        tickStopped: isTickStopped, watchdogResets, crashLog,
    };
}

/** Chip time as a percentage of wall time, over the last second or so. */
function measureSpeed(nowMs) {
    if ((nowMs - speed.wallMs) >= SPEED_WINDOW_MS) {
        const ratio = (chipMs() - speed.chipMs) / (nowMs - speed.wallMs);

        speed = { wallMs: nowMs, chipMs: chipMs(), percent: Math.round(ratio * 100) };
    }
    return speed.percent;
}

/** Lets the run button go once it has been held down long enough. */
function releaseRunWhenDue() {
    if ((runUpDueMs !== null) && (chipMs() >= runUpDueMs)) {
        board.runButton.set(false);
        runUpDueMs = null;
        runFreeMs = chipMs() + RUN_HOLD_MS;
        broadcast('state', stateMessage(false));
    }
}

/**
 * Adds a `crash` event if the crash log in the EEPROM is not as it was when
 * last looked at. A record is written a byte at a time, 3.6 ms each, so one
 * can be seen arriving over a slice or two.
 */
function addCrashLogChange(events) {
    const log = board.crashLog();

    if (JSON.stringify(log) !== JSON.stringify(crashLog)) {
        crashLog = log;
        events.push({ t: chipMs(), k: 'crash', v: log });
    }
}

/** Runs the chip on to where a real one would be by now. */
function runSlice() {
    const nowMs = performance.now();
    let dueMs = (nowMs - wallStartMs) - chipMs();
    let events = null;

    if (dueMs > MAX_SLICE_CHIP_MS) {
        // The PC is too slow, or was busy: let the lag go, not chase it
        wallStartMs += dueMs - MAX_SLICE_CHIP_MS;
        dueMs = MAX_SLICE_CHIP_MS;
    }
    if (isTickStopped) {
        // Asked again every slice until the reset comes: the last slice may
        // have ended inside the tick interrupt, where asking does nothing
        board.stopTick();
    }
    if (dueMs > 0) {
        board.runForMs(dueMs);
    }
    events = drain();
    addCrashLogChange(events);
    broadcast('run', { ms: chipMs(), speed: measureSpeed(nowMs), events });
    if (events.some((event) => event.k === 'watchdog')) {
        // The mode, the transport and the count of resets have changed
        broadcast('state', stateMessage(false));
    }
    releaseRunWhenDue();
}

function startRunning() {
    // Carry on from where the chip stopped, not from where it would be
    wallStartMs = performance.now() - chipMs();
    speed = { wallMs: performance.now(), chipMs: chipMs(), percent: 100 };
    timer = setInterval(runSlice, SLICE_MS);
}

function isWholeNumberUpTo(value, max) {
    return Number.isInteger(value) && (value >= 0) && (value <= max);
}

function isNotes(value) {
    return Array.isArray(value) && (value.length === STEP_COUNT)
        && value.every((note) => isWholeNumberUpTo(note, MAX_NOTE));
}

/**
 * Says what the firmware will make of a run button press, since the button
 * only toggles and the release image never says which way it went: stopped
 * becomes running, from the first step, and running becomes stopped. It is
 * a prediction from the presses made here, each held long enough to count;
 * the debug image's `Transport = N` line then confirms or corrects it.
 */
function predictRunPress() {
    if (isCalibrating) {
        return; // Nothing is sequenced, so the button does nothing
    }
    if (outputs.transport === TRANSPORT_STOPPED) {
        outputs.transport = runningTransport();
        isFirstStepDue = true;
    } else {
        outputs.transport = TRANSPORT_STOPPED;
    }
    outputs.transportFrom = 'controls';
}

/** Presses the run button, unless it is still down or only just let go. */
function pressRun() {
    if (board.runButton.isClosed() || (chipMs() < runFreeMs)) {
        return;
    }
    board.runButton.set(true);
    runUpDueMs = chipMs() + RUN_HOLD_MS;
    predictRunPress();
}

function setSync(isOn) {
    panel.sync = isOn;
    board.setSyncSwitch(isOn);
    // While running, the switch chooses the clock; stopped stays stopped
    if (!isCalibrating && (outputs.transport !== TRANSPORT_STOPPED)) {
        outputs.transport = runningTransport();
        outputs.transportFrom = 'controls';
    }
}

/** Switches the clock source on or off. Switching on gives an edge at once. */
function setClockOn(isOn) {
    if (isOn !== panel.clockOn) {
        panel.clockOn = isOn;
        if (isOn) {
            board.startClockIn(panel.clockMs, panel.clockPattern);
        } else {
            board.stopClockIn();
        }
    }
}

/**
 * Sets the clock source's period or its pattern. A running clock starts
 * again with it, a random pattern from the start of its train.
 */
function setClock(periodMs, pattern) {
    panel.clockMs = periodMs;
    panel.clockPattern = pattern;
    if (panel.clockOn) {
        board.startClockIn(periodMs, pattern);
    }
}

function pulseJack(jack) {
    if (jack === 'reset') {
        board.pulseResetIn(JACK_PULSE_MS);
        // A reset is taken only while running, and plays the first step at
        // the next step boundary
        if (!isCalibrating && (outputs.transport !== TRANSPORT_STOPPED)) {
            isFirstStepDue = true;
        }
    } else if (!panel.clockOn) {
        // With the clock source running, the jack is already taken
        board.pulseClockIn(JACK_PULSE_MS);
    }
}

/** Applies one change from a page. Returns false if it is not a valid one. */
function applyCommand(command) {
    let isFresh = false;

    if ((typeof command !== 'object') || (command === null)) {
        return false;
    }
    if (isNotes(command.notes)) {
        panel.notes = command.notes;
        board.setNotes(panel.notes);
    } else if (isWholeNumberUpTo(command.mode, MAX_MODE)) {
        panel.mode = command.mode;
        board.setModeSwitches(panel.mode);
    } else if (isWholeNumberUpTo(command.tempo, MAX_TEMPO_READING)) {
        panel.tempo = command.tempo;
        board.setTempoReading(panel.tempo);
    } else if (typeof command.sync === 'boolean') {
        setSync(command.sync);
    } else if (command.run === 'press') {
        pressRun();
    } else if ((command.pulse === 'clock') || (command.pulse === 'reset')) {
        pulseJack(command.pulse);
    } else if (typeof command.clockOn === 'boolean') {
        setClockOn(command.clockOn);
    } else if (isWholeNumberUpTo(command.clockMs, MAX_CLOCK_MS)
        && (command.clockMs >= MIN_CLOCK_MS)) {
        setClock(command.clockMs, panel.clockPattern);
    } else if (CLOCK_PATTERNS.includes(command.clockPattern)) {
        setClock(panel.clockMs, command.clockPattern);
    } else if (command.tick === 'stop') {
        board.stopTick();
        isTickStopped = true;
    } else if (CONFIGS.includes(command.power)) {
        const isErased = command.keepEeprom === false;

        powerUp(command.power, isErased ? [] : board.eepromBytes(EEPROM_BYTES));
        isFresh = true;
    } else {
        return false;
    }
    broadcast('state', stateMessage(isFresh));
    return true;
}

function reply(res, status, text) {
    res.writeHead(status, { 'Content-Type': 'text/plain; charset=utf-8' });
    res.end(text);
}

function servePage(res) {
    res.writeHead(200, {
        'Content-Type': 'text/html; charset=utf-8',
        'Cache-Control': 'no-store',
    });
    res.end(fs.readFileSync(PAGE_PATH));
}

function openStream(req, res) {
    res.writeHead(200, { 'Content-Type': 'text/event-stream', 'Cache-Control': 'no-store' });
    clients.add(res);
    send(res, 'state', stateMessage(true));
    if (timer === null) {
        startRunning();
    }
    req.on('close', () => {
        clients.delete(res);
        if (clients.size === 0) {
            clearInterval(timer);
            timer = null;
        }
    });
}

function takeCommand(req, res) {
    let body = '';

    // Only a page that can set this header may post, which a form on some
    // other site cannot
    if (req.headers['content-type'] !== 'application/json') {
        reply(res, 415, 'JSON only');
        return;
    }
    req.on('data', (chunk) => {
        body += chunk;
        if (body.length > MAX_BODY_BYTES) {
            req.destroy();
        }
    });
    req.on('end', () => {
        try {
            const isValid = applyCommand(JSON.parse(body));

            reply(res, isValid ? 204 : 400, isValid ? '' : 'Not a valid change');
        } catch (error) {
            reply(res, 400, error.message);
        }
    });
}

function handle(req, res) {
    if ((req.method === 'GET') && (req.url === '/')) {
        servePage(res);
    } else if ((req.method === 'GET') && (req.url === '/events')) {
        openStream(req, res);
    } else if ((req.method === 'POST') && (req.url === '/control')) {
        takeCommand(req, res);
    } else {
        reply(res, 404, 'Not found');
    }
}

function main() {
    const port = Number(process.argv[2]) || DEFAULT_PORT;

    powerUp(panel.config, []);
    http.createServer(handle).listen(port, HOST, () => {
        console.log(`Emulated board at http://localhost:${port}/  (Ctrl+C stops it)`);
    });
}

main();
