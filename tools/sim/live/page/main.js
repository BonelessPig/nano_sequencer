'use strict';
// The connection to the server and the controls' handlers; loaded last
// ---- The server ----

// What the log pane says for an image that sends no log
function noLogText() {
    return state.sendsMidi
        ? '(the midi image has no log: its serial pin carries MIDI, shown under MIDI out)'
        : `(the ${state.config} image has no log)`;
}

function applyState(newState) {
    state = newState;
    if (state.fresh) {
        // A power-up, or this page has only just connected
        nowMs = state.ms;
        trace.cv = [[state.ms, state.millivolts]];
        trace.gate = [[state.ms, state.gate ? 1 : 0]];
        trace.clock = [[state.ms, state.clock ? 1 : 0]];
        trace.clockIn = [];
        trace.resetIn = [];
        lastClockRiseMs = null;
        el('log').textContent = state.hasLog ? '' : noLogText();
        el('midi-log').textContent = '';
        el('bpm').textContent = '';
        el('config').value = state.config;
        el('clock-led').classList.toggle('lit', state.clock);
        showGate(state.gate);
        showCv(state.millivolts);
        showEnv(state.envMillivolts);
    }
    el('cal-banner').hidden = !state.calibrating;
    showPanel();
    showTransport();
    showStep();
    showLeds();
    showWatchdog();
    showCrashLog();
    showMidi();
}

function applyRun(run) {
    nowMs = run.ms;
    run.events.forEach(applyEvent);
    el('level-led').style.opacity = String(run.level);
    el('level-reading').textContent = `${Math.round(run.level * 100)}%`;
    el('status').textContent = `Running ${state.config}: ${(run.ms / 1000).toFixed(1)} s `
        + `of chip time, at ${run.speed}% of real speed`;
}

function connect() {
    const source = new EventSource('/events');

    source.addEventListener('state', (message) => applyState(JSON.parse(message.data)));
    source.addEventListener('run', (message) => applyRun(JSON.parse(message.data)));
    source.onerror = () => {
        el('status').textContent = 'Not connected: is `make live` still running?';
    };
}

buildPanel();
buildPots();
el('tempo').addEventListener('input', () => post({ tempo: Number(el('tempo').value) }));
el('lfo').addEventListener('input', () => post({ lfo: Number(el('lfo').value) }));
el('lfo-centre').addEventListener('click', () => post({ lfo: LFO_CENTRE }));
function powerCycle() {
    post({ power: el('config').value, keepEeprom: el('keep-eeprom').checked });
}

el('run').addEventListener('click', () => post({ run: 'press' }));
el('clock-on').addEventListener('change', () => post({ clockOn: el('clock-on').checked }));
// The rate is posted when the pot is let go: a running clock starts
// again at each new period, with an edge at once
el('clock-ms').addEventListener('input', () => showClockRate(Number(el('clock-ms').value)));
el('clock-ms').addEventListener('change', () => {
    el('clock-ms').blur(); // So the answer from the server sets it
    post({ clockMs: Number(el('clock-ms').value) });
});
el('clock-pattern').addEventListener('change',
    () => post({ clockPattern: el('clock-pattern').value }));
el('clock-pulse').addEventListener('click', () => post({ pulse: 'clock' }));
el('reset-pulse').addEventListener('click', () => post({ pulse: 'reset' }));
el('stop-tick').addEventListener('click', () => post({ tick: 'stop' }));
el('power').addEventListener('click', powerCycle);
// Choosing an image is a power cycle with it: until one, the image named
// would not be the one running
el('config').addEventListener('change', powerCycle);
el('gear-power').addEventListener('click', toggleGear);
el('vco-wave').addEventListener('change', applyGear);
for (const id of ['vco-tune', 'vco-position', 'chorus', 'env-attack', 'env-release',
    'env-position', 'volume']) {
    el(id).addEventListener('input', applyGear);
}
window.addEventListener('resize', drawWavetable);
applyGear();
el('cal-enter').addEventListener('click', () => {
    const notes = state.notes.slice();

    CAL_ENTRY_NOTES.forEach((note, i) => {
        notes[STEP_COUNT - CAL_ENTRY_NOTES.length + i] = note;
    });
    // The pattern has to be on the panel before the board powers up
    post({ notes }).then(powerCycle);
});
connect();
requestAnimationFrame(drawTrace);
requestAnimationFrame(drawScope);
requestAnimationFrame(drawPots);
