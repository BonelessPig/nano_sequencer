'use strict';
/**
 * The emulated board, live (`make live`): runs one firmware image at the pace
 * of a real chip and serves a page that shows what the board does and lets
 * its panel be worked. The chip and the parts are the same ones the scenarios
 * use; only this folder knows about a browser.
 *
 *   GET  /         The page: page.html, with each section kept in page/ put
 *                  in where its `part:` comment stands.
 *   GET  /name.css, /name.js   The page's style and scripts, from page/.
 *   GET  /events   A stream of server-sent events: `state` (the panel, which
 *                  image is running, its mode, the output levels (the two
 *                  pins, the pitch CV and the envelope CV) and the
 *                  step lights) when a page
 *                  connects and when a control changes, and `run` every
 *                  slice with the chip time and what happened in it.
 *                  What the midi image sent is among that: a `midi` event
 *                  for each Start, Stop, Note On and Note Off, and one for
 *                  a slice's clocks however many there were.
 *   POST /control  One change, as JSON:
 *                  { notes: [16 values, 0 to 15] }
 *                  { mode: 0 to 255 }           The eight mode switches, a
 *                                               bit a switch.
 *                  { tempo: 0 to 1023 }             The pot.
 *                  { selector: 0 to 7 }             What the pot sets.
 *                  { lfo: 0 to 1023 }               The modulation input,
 *                                               512 its centre.
 *                  { rate: 0 to 1023 }              The rate pot of the
 *                                               board's own LFO.
 *                  { profile: true | false }    The profile button, down
 *                                               or let go.
 *                  { run: 'press' }             One press of the run button.
 *                  { pulse: 'clock' | 'reset' } One pulse on that input jack.
 *                  { clockOn: true | false }    The clock source plugged
 *                                               into the clock input jack.
 *                  { clockMs: 20 to 1000 }      Its period.
 *                  { clockPattern: name }       How even its edges are:
 *                                               one of the board's
 *                                               CLOCK_PATTERNS.
 *                  { midiClockOn }, { midiBpm }, { midiIn }   What is
 *                                               plugged into MIDI in: see
 *                                               midi_in.js.
 *                  { tick: 'stop' }             Stops the firmware's tick,
 *                                               for the watchdog to catch.
 *                  { power: 'debug' | 'release' | 'midi',
 *                    keepEeprom: true | false }, which is a power cycle.
 *
 * Unlike a scenario this is paced by the PC's clock, so no two runs are the
 * same; nothing is asserted here. The chip stops while no page is connected.
 *
 * Two things shown are worked out, not read from the chip: the transport
 * and the step that is playing. That is predict.js, which record() and the
 * controls here tell what happened.
 *
 * MIDI out is not worked out: the messages are the board's, read from the
 * bytes the chip sent. What is kept of them for a page to show is in
 * midi_out.js.
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
const http = require('node:http');
const {
    createBoard, CONFIGS, STEP_COUNT, CLOCK_PATTERNS,
} = require('../boards/nano_sequencer');
const {
    STEP_UNKNOWN, startAfresh, isInCalibrationMode, takePanelRead, nextStep,
    followLog, takeExternalEdge, takeMidiIn, predictRunPress, predictReset,
} = require('./predict');
const {
    midiSummary, recordMidi, forgetMidi, beginMidiSlice, addMidiClocks,
    addMidiClockQuiet,
} = require('./midi_out');
const { plugMidiIn, applyMidiInCommand } = require('./midi_in');
const { reply, servePage, serveAsset, ASSET_URL } = require('./page_files');

const HOST = '127.0.0.1'; // This PC only
const DEFAULT_PORT = 8165;
const SLICE_MS = 10;         // How often the chip is run on and the page told
const MAX_SLICE_CHIP_MS = 50; // The most chip time one slice may catch up
const SPEED_WINDOW_MS = 1000;
const EEPROM_BYTES = 1024;
const POWER_ON_RESET = 0x01;
const MAX_NOTE = 15;
const MAX_MODE = 255;
const MAX_TEMPO_READING = 1023;
const MAX_SELECTOR = 7;
const MAX_LFO_READING = 1023;
const MAX_RATE_READING = 1023;
const LFO_CENTRE_READING = 512; // An LFO at its centre, or none plugged in
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
const TRANSPORT_LINE = /^Transport = ([0-2])$/;
const STEP_LINE = /^Step (\d+) /;

// Where the controls stand. They outlast a power cycle, as the real ones do
const panel = {
    config: CONFIGS[0],
    notes: START_NOTES.slice(),
    mode: 0,
    tempo: START_TEMPO_READING,
    selector: 0,
    lfo: LFO_CENTRE_READING,
    rate: 0,
    profileDown: false,
    clockOn: false,
    clockMs: START_CLOCK_MS,
    clockPattern: CLOCK_PATTERNS[0], // Steady
    midiClockOn: false,
    midiBpm: 120,
};
const clients = new Set();
let board = null;
let outputs = null;
let runUpDueMs = null;     // Chip time at which the run button is let go
let profileDownMs = null;  // Chip time at which the profile button went down
let isStatusLit = false;   // The status light, as the last slice left it
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

/** Starts a board from reset with the panel as it stands. */
function powerUp(config, eeprom) {
    board = createBoard(config, { eeprom, resetFlags: POWER_ON_RESET });
    board.setNotes(panel.notes);
    board.setModeSwitches(panel.mode);
    board.setTempoReading(panel.tempo);
    board.setSelector(panel.selector);
    board.setLfoReading(panel.lfo);
    board.setRateReading(panel.rate);
    board.setProfileButton(panel.profileDown);
    profileDownMs = panel.profileDown ? 0 : null;
    isStatusLit = false;
    // Whatever is plugged into the clock input carries on; the run button
    // is up
    if (panel.clockOn) {
        board.startClockIn(panel.clockMs, panel.clockPattern);
    }
    plugMidiIn(board, panel);
    panel.config = config;
    outputs = {
        gate: false,
        clock: false,
        step: STEP_UNKNOWN,
        stageStep: 0,   // The stage engine: which step of its stage, from 1,
        stageSteps: 0,  // and how many steps the stage lasts
        millivolts: 0,
        envMillivolts: 0, // The envelope CV, the DAC's channel B
        leds: 0,        // The step lights as last latched, bit n for step n
        transport: null, // These two, the step and the stage's count are
        transportFrom: 'controls', // worked out: see startAfresh()
    };
    // The firmware chooses its mode once, from the panel as it is now. This
    // is worked out from the panel, not read from the chip
    startAfresh(outputs, panel, board.readsCalibrationEntry());
    forgetMidi();
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
 * start-up is not a press. What is kept of MIDI out is left as well: the
 * Start the firmware sends as it comes up begins the count of clocks again,
 * and a note that was sounding is still sounding at the receiver, since no
 * Note Off was sent for it.
 */
function startAgain() {
    startAfresh(outputs, panel, board.readsCalibrationEntry());
    isTickStopped = false;
    isStatusLit = false;
    watchdogResets++;
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
        case 'env':
            // Written every tick too, and told the same way
            if (item.value !== outputs.envMillivolts) {
                outputs.envMillivolts = item.value;
                events.push({ t, k: 'env', v: item.value });
            }
            break;
        case 'read':
            takePanelRead();
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
        case 'midi':
            recordMidi(item.value, t, events);
            break;
        case 'watchdog':
            // The controls only change between slices, so the firmware
            // started again with the panel as it stands now
            startAgain();
            events.push({ t, k: 'watchdog' });
            break;
        case 'midiin':
            if (board.hearsMidi) {
                takeMidiIn(item.value, t, events);
            }
            break;
        case 'clockin':
            events.push({ t, k: item.kind });
            takeExternalEdge(t, events);
            break;
        default:
            // An edge on the reset input
            events.push({ t, k: item.kind });
            break;
    }
}

/**
 * Takes everything the board has recorded since the last call, in the order
 * it happened, and empties the records so a long run does not fill memory.
 */
function drain() {
    const { serial, midi } = board;
    const items = [];
    const events = [];
    const add = (cycle, kind, value) => items.push({ cycle, kind, value });

    board.gate.edges.forEach((edge) => add(edge.cycle, 'gate', edge.high));
    board.clockOut.edges.forEach((edge) => add(edge.cycle, 'clock', edge.high));
    board.cvWrites.forEach((write) => add(write.cycle, 'cv', write.millivolts));
    board.envWrites.forEach((write) => add(write.cycle, 'env', write.millivolts));
    board.ledLatches.forEach((latch) => add(latch.cycle, 'led', latch.value));
    board.panelReads.forEach((read) => add(read.cycle, 'read'));
    board.clockIn.rises.forEach((cycle) => add(cycle, 'clockin'));
    board.midiIn.sent.forEach((byte) => add(byte.cycle, 'midiin', byte.value));
    board.resetIn.rises.forEach((cycle) => add(cycle, 'resetin'));
    board.resets.forEach((cycle) => add(cycle, 'watchdog'));
    for (const line of [...serial.lines, ...serial.notices]) {
        add(line.startCycle, 'log', line.text);
    }
    midi.messages.forEach((message) => add(message.cycle, 'midi', message));
    // A step's clock pulse comes ahead of its log line, and stays ahead
    items.sort((a, b) => a.cycle - b.cycle);
    beginMidiSlice();
    items.forEach((item) => record(item, events));
    addMidiClocks(events);
    addMidiClockQuiet(chipMs(), events);
    if (board.statusLight.edges.length > 0) {
        isStatusLit = board.statusLight.edges[board.statusLight.edges.length - 1].high;
    }

    for (const list of [board.gate.edges, board.clockOut.edges, board.statusLight.edges,
        board.cvWrites,
        board.envWrites,
        board.cvIgnoredFrames, board.panelReads, board.spiSent,
        board.ledLatches, board.clockIn.rises, board.resetIn.rises,
        board.midiIn.sent,
        board.resets, serial.lines, serial.notices, midi.bytes, midi.messages]) {
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
        calibrating: isInCalibrationMode(), ms: chipMs(), fresh: isFresh,
        hasLog: board.hasLog, sendsMidi: board.sendsMidi, midi: midiSummary(),
        hearsMidi: board.hearsMidi,
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
    // The level light is switched too fast to send edge by edge: what goes
    // out is how much of the slice it was on for
    broadcast('run', {
        ms: chipMs(), speed: measureSpeed(nowMs), level: board.takeLevel(),
        status: isStatusLit, events,
        profileHeldMs: (profileDownMs === null) ? null : chipMs() - profileDownMs,
    });
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

/** Presses the run button, unless it is still down or only just let go. */
function pressRun() {
    if (board.runButton.isClosed() || (chipMs() < runFreeMs)) {
        return;
    }
    board.runButton.set(true);
    runUpDueMs = chipMs() + RUN_HOLD_MS;
    predictRunPress();
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
        predictReset();
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
    } else if (isWholeNumberUpTo(command.selector, MAX_SELECTOR)) {
        panel.selector = command.selector;
        board.setSelector(panel.selector);
    } else if (isWholeNumberUpTo(command.lfo, MAX_LFO_READING)) {
        panel.lfo = command.lfo;
        board.setLfoReading(panel.lfo);
    } else if (isWholeNumberUpTo(command.rate, MAX_RATE_READING)) {
        panel.rate = command.rate;
        board.setRateReading(panel.rate);
    } else if (typeof command.profile === 'boolean') {
        if (command.profile !== panel.profileDown) {
            panel.profileDown = command.profile;
            board.setProfileButton(panel.profileDown);
            profileDownMs = panel.profileDown ? chipMs() : null;
        }
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
    } else if (applyMidiInCommand(command, board, panel)) {
        // MIDI in's own: the clock into it, or a message
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
    const asset = ASSET_URL.exec(req.url);

    if ((req.method === 'GET') && (req.url === '/')) {
        servePage(res);
    } else if ((req.method === 'GET') && (asset !== null)) {
        serveAsset(res, asset[1], asset[2]);
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
