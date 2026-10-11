'use strict';
/**
 * The emulated board, live (`make live`): runs one firmware image at the pace
 * of a real chip and serves a page that shows what the board does and lets
 * its panel be worked. The chip and the parts are the same ones the scenarios
 * use; only this file and page.html know about a browser.
 *
 *   GET  /         The page.
 *   GET  /events   A stream of server-sent events: `state` (the panel, which
 *                  image is running and the output levels) when a page
 *                  connects and when a control changes, and `run` every
 *                  slice with the chip time and what happened in it.
 *   POST /control  One change, as JSON: { notes: [16 values, 0 to 15] },
 *                  { tempo: 0 to 1023 } or { power: 'debug' | 'release',
 *                  keepEeprom: true | false }, which is a power cycle.
 *
 * Unlike a scenario this is paced by the PC's clock, so no two runs are the
 * same; nothing is asserted here. The chip stops while no page is connected.
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

const panel = {
    config: CONFIGS[0],
    notes: START_NOTES.slice(),
    tempo: START_TEMPO_READING,
};
const clients = new Set();
let board = null;
let outputs = null;
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
    board.setTempoReading(panel.tempo);
    panel.config = config;
    outputs = { gate: false, clock: false, step: -1, millivolts: 0 };
    wallStartMs = performance.now();
    speed = { wallMs: wallStartMs, chipMs: 0, percent: 100 };
}

/**
 * Takes everything the board has recorded since the last call, in the order
 * it happened, and empties the records so a long run does not fill memory.
 */
function drain() {
    const { machine, serial } = board;
    const events = [];
    const at = (cycle) => machine.cyclesToMs(cycle);

    for (const edge of board.gate.edges) {
        outputs.gate = edge.high;
        events.push({ t: at(edge.cycle), k: 'gate', v: edge.high });
    }
    for (const edge of board.clockOut.edges) {
        outputs.clock = edge.high;
        if (edge.high) {
            // The clock output pulses once a step, from step 0
            outputs.step = (outputs.step + 1) % STEP_COUNT;
        }
        events.push({ t: at(edge.cycle), k: 'clock', v: edge.high, step: outputs.step });
    }
    // The DAC is written every tick; only a change is worth telling
    for (const write of board.cvWrites) {
        if (write.millivolts !== outputs.millivolts) {
            outputs.millivolts = write.millivolts;
            events.push({ t: at(write.cycle), k: 'cv', v: write.millivolts });
        }
    }
    for (const line of [...serial.lines, ...serial.notices]) {
        events.push({ t: at(line.startCycle), k: 'log', v: line.text });
    }

    for (const list of [board.gate.edges, board.clockOut.edges, board.cvWrites,
        board.cvIgnoredFrames, board.panelReads, board.spiSent,
        serial.lines, serial.notices]) {
        list.length = 0;
    }
    return events.sort((a, b) => a.t - b.t);
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
    return { ...panel, ...outputs, ms: chipMs(), fresh: isFresh };
}

/** Chip time as a percentage of wall time, over the last second or so. */
function measureSpeed(nowMs) {
    if ((nowMs - speed.wallMs) >= SPEED_WINDOW_MS) {
        const ratio = (chipMs() - speed.chipMs) / (nowMs - speed.wallMs);

        speed = { wallMs: nowMs, chipMs: chipMs(), percent: Math.round(ratio * 100) };
    }
    return speed.percent;
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
