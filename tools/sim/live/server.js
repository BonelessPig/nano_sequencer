'use strict';
/**
 * The emulated board, live (`make live`): runs one firmware image at the pace
 * of a real chip and serves a page that shows what the board does and lets
 * its panel be worked. The chip and the parts are the same ones the scenarios
 * use; only this file and page.html know about a browser.
 *
 *   GET  /         The page.
 *   GET  /events   A stream of server-sent events: `state` (the panel, which
 *                  image is running, its mode and the output levels) when a page
 *                  connects and when a control changes, and `run` every
 *                  slice with the chip time and what happened in it.
 *   POST /control  One change, as JSON:
 *                  { notes: [16 values, 0 to 15] }
 *                  { tempo: 0 to 1023 }
 *                  { sync: true | false }       The sync switch.
 *                  { run: 'press' }             One press of the run button.
 *                  { pulse: 'clock' | 'reset' } One pulse on that input jack.
 *                  { clockOn: true | false }    The clock source plugged
 *                                               into the clock input jack.
 *                  { clockMs: 20 to 1000 }      Its period.
 *                  { power: 'debug' | 'release', keepEeprom: true | false },
 *                  which is a power cycle.
 *
 * Unlike a scenario this is paced by the PC's clock, so no two runs are the
 * same; nothing is asserted here. The chip stops while no page is connected.
 *
 * Two things shown are worked out here, not read from the chip: the
 * transport and the step that is playing. Both follow the log where there is
 * one and what this server did to the controls where there is not; see
 * predictRunPress() and record().
 */
const fs = require('node:fs');
const http = require('node:http');
const path = require('node:path');
const { createBoard, STEP_COUNT } = require('../boards/nano_sequencer');

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
    tempo: START_TEMPO_READING,
    sync: false,
    clockOn: false,
    clockMs: START_CLOCK_MS,
};
const clients = new Set();
let board = null;
let outputs = null;
let isCalibrating = false; // Powered up with the calibration entry pattern set
let isFirstStepDue = false; // The next step to begin is the pattern's first
let runUpDueMs = null;     // Chip time at which the run button is let go
let runFreeMs = 0;         // Chip time from which it may be pressed again
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
    board.setTempoReading(panel.tempo);
    // A toggle switch stays where it was and whatever is plugged into the
    // clock input carries on; the run button is up
    board.setSyncSwitch(panel.sync);
    if (panel.clockOn) {
        board.startClockIn(panel.clockMs);
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
        millivolts: 0,
        transport: isCalibrating ? null : runningTransport(),
        transportFrom: 'controls',
    };
    isFirstStepDue = true;
    runUpDueMs = null;
    runFreeMs = 0;
    wallStartMs = performance.now();
    speed = { wallMs: wallStartMs, chipMs: 0, percent: 100 };
}

/**
 * Takes one thing the board recorded: brings `outputs` up to date with it
 * and adds what the page should be told to `events`.
 *
 * The step playing is counted from the clock output, which pulses once a
 * step, starting again from 0 when this server has done something that
 * sends the pattern back to its first step. The debug image also names each
 * step in its log, just after the pulse, and that is believed over the count.
 */
function record(item, events) {
    const t = board.machine.cyclesToMs(item.cycle);
    const setStep = (step) => {
        outputs.step = step % STEP_COUNT;
        events.push({ t, k: 'step', v: outputs.step });
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
                setStep(isFirstStepDue ? 0 : outputs.step + 1);
                isFirstStepDue = false;
            }
            break;
        case 'cv':
            // The DAC is written every tick; only a change is worth telling
            if (item.value !== outputs.millivolts) {
                outputs.millivolts = item.value;
                events.push({ t, k: 'cv', v: item.value });
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
            if (step !== null) {
                setStep(Number(step[1]));
            }
            break;
        }
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
    board.clockIn.rises.forEach((cycle) => add(cycle, 'clockin'));
    board.resetIn.rises.forEach((cycle) => add(cycle, 'resetin'));
    for (const line of [...serial.lines, ...serial.notices]) {
        add(line.startCycle, 'log', line.text);
    }
    // A step's clock pulse comes ahead of its log line, and stays ahead
    items.sort((a, b) => a.cycle - b.cycle);
    items.forEach((item) => record(item, events));

    for (const list of [board.gate.edges, board.clockOut.edges, board.cvWrites,
        board.cvIgnoredFrames, board.panelReads, board.spiSent,
        board.clockIn.rises, board.resetIn.rises,
        serial.lines, serial.notices]) {
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

/** Runs the chip on to where a real one would be by now. */
function runSlice() {
    const nowMs = performance.now();
    let dueMs = (nowMs - wallStartMs) - chipMs();

    if (dueMs > MAX_SLICE_CHIP_MS) {
        // The PC is too slow, or was busy: let the lag go, not chase it
        wallStartMs += dueMs - MAX_SLICE_CHIP_MS;
        dueMs = MAX_SLICE_CHIP_MS;
    }
    if (dueMs > 0) {
        board.runForMs(dueMs);
    }
    broadcast('run', { ms: chipMs(), speed: measureSpeed(nowMs), events: drain() });
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
            board.startClockIn(panel.clockMs);
        } else {
            board.stopClockIn();
        }
    }
}

/** Sets the clock source's period. A running clock starts again at it. */
function setClockMs(periodMs) {
    panel.clockMs = periodMs;
    if (panel.clockOn) {
        board.startClockIn(periodMs);
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
        setClockMs(command.clockMs);
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
