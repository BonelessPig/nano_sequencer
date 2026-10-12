'use strict';
// The watchdog and crash log, the outputs, MIDI out, and each event applied
// ---- Watchdog and crash log ----

// Whether the tick has been stopped and the reset is still to come, and
// how many watchdog resets there have been since power-up, as the server
// counts them from the chip
function showWatchdog() {
    const resets = state.watchdogResets;
    let text = '';

    if (state.tickStopped) {
        text = 'Tick stopped: waiting for the watchdog';
    } else if (resets > 0) {
        text = `${resets} watchdog ${(resets === 1) ? 'reset' : 'resets'} since power-up`;
    } else {
        // Nothing has happened to say
    }
    el('watchdog').textContent = text;
    el('stop-tick').disabled = state.tickStopped;
}

// The crash log as the board read it from the EEPROM: a row for each slot
// that holds a record, the newest marked
function showCrashLog() {
    const { newest, records } = state.crashLog;
    const rows = el('crash-rows');

    rows.textContent = '';
    for (const record of records) {
        const row = rows.insertRow();
        const place = PLACE_NAMES[record.place] ?? PLACE_UNMARKED;
        const cause = (record.cause === CAUSE_WATCHDOG) ? `${record.cause}, watchdog`
            : String(record.cause);

        for (const text of [record.slot, record.number, cause,
            `${record.place}, ${place}`, record.count,
            (record.slot === newest) ? 'newest' : '']) {
            row.insertCell().textContent = String(text);
        }
        row.className = 'num';
    }
    el('crash-log').hidden = records.length === 0;
    el('crash-log-empty').hidden = records.length !== 0;
}

// ---- Outputs ----

function showGate(isHigh) {
    el('gate-led').classList.toggle('lit', isHigh);
    gearGate(isHigh);
}

function showEnv(millivolts) {
    el('env').textContent = `${(millivolts / 1000).toFixed(3)} V`;
}

function showCv(millivolts) {
    el('cv').textContent = `${(millivolts / 1000).toFixed(3)} V`;
    gearPitch(millivolts);
}

function showClock(event) {
    el('clock-led').classList.toggle('lit', event.v);
    if (event.v) {
        if (lastClockRiseMs !== null) {
            const bpm = MS_PER_MINUTE / ((event.t - lastClockRiseMs) * STEPS_PER_BEAT);

            el('bpm').textContent = `${bpm.toFixed(1)} BPM measured`;
        }
        lastClockRiseMs = event.t;
    }
}

// ---- MIDI out ----

// The clocks and the notes sounding, as the server keeps them from the
// messages: `midi` in the state, brought up to date by every `midi` event
function showMidi() {
    const { clocks, bpm, sounding } = state.midi;
    const quarters = Math.floor(clocks / MIDI_CLOCKS_PER_QUARTER);
    let clockText = `The ${state.config} image sends no MIDI`;
    let soundingText = '';

    if (state.sendsMidi && state.calibrating) {
        clockText = 'Calibration mode: no MIDI is sent';
    } else if (state.sendsMidi) {
        clockText = (bpm === null)
            ? 'Clock: no tempo to give' : `Clock: ${bpm.toFixed(1)} BPM`;
        clockText += `, ${clocks} since Start (${quarters} quarter `
            + `${(quarters === 1) ? 'note' : 'notes'})`;
        soundingText = `Sounding: ${(sounding.length === 0) ? 'none'
            : sounding.map((one) => `${one.name} (${one.note})`).join(', ')}`;
    } else {
        // An image with nothing on the wire that is MIDI
    }
    el('midi-clock').textContent = clockText;
    el('midi-sounding').textContent = soundingText;
}

// One line of the MIDI monitor for a message that is not a clock
function midiLine(event) {
    const time = `${(event.t / 1000).toFixed(3).padStart(9)} s  `;

    if (event.note !== undefined) {
        return `${time}${MIDI_NAMES[event.v]}  ${event.name.padEnd(3)} (${event.note})`
            + `  velocity ${event.vel}, channel ${event.ch}`;
    }
    if (MIDI_NAMES[event.v] !== undefined) {
        return time + MIDI_NAMES[event.v];
    }
    return `${time}Not a message the firmware should send: byte ${event.byte}`;
}

function addLine(id, text, max) {
    const pane = el(id);
    const lines = (pane.textContent === '') ? [] : pane.textContent.split('\n').slice(-max);

    lines.push(text);
    pane.textContent = lines.join('\n');
    pane.scrollTop = pane.scrollHeight;
}

// A `midi` event: a line for a message, none for a slice's clocks or for
// the clocks having stopped, and what the server keeps for all three
function applyMidi(event) {
    state.midi = event.now;
    if ((event.v !== 'clock') && (event.v !== 'quiet')) {
        addLine('midi-log', midiLine(event), MIDI_LINES_MAX);
    }
    showMidi();
}

function addLogLine(text) {
    addLine('log', text, LOG_LINES_MAX);
}

function applyEvent(event) {
    switch (event.k) {
        case 'gate':
            trace.gate.push([event.t, event.v ? 1 : 0]);
            showGate(event.v);
            break;
        case 'clock':
            trace.clock.push([event.t, event.v ? 1 : 0]);
            showClock(event);
            break;
        case 'cv':
            trace.cv.push([event.t, event.v]);
            showCv(event.v);
            break;
        case 'env':
            showEnv(event.v);
            break;
        case 'led':
            state.leds = event.v;
            showLeds();
            break;
        case 'log':
            addLogLine(event.v);
            break;
        case 'midi':
            applyMidi(event);
            break;
        case 'step':
            state.step = event.v;
            state.stageStep = event.n;
            state.stageSteps = event.of;
            showStep();
            break;
        case 'transport':
            state.transport = event.v;
            state.transportFrom = event.from;
            el('tempo-means').textContent = tempoMeaning();
            showTransport();
            showStep();
            break;
        case 'clockin':
            trace.clockIn.push(event.t);
            break;
        case 'resetin':
            trace.resetIn.push(event.t);
            break;
        case 'crash':
            state.crashLog = event.v;
            showCrashLog();
            break;
        case 'watchdog':
            // The chip was reset: the next clock pulse is not a step on
            // from the last. The `state` that follows has the rest
            lastClockRiseMs = null;
            el('bpm').textContent = '';
            break;
        default:
            break;
    }
}
