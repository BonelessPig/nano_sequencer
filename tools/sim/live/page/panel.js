'use strict';
// The panel: the steps, the engine and the mode switches, the transport.
// What draws its two rotary switches is in pots.js
// ---- Panel ----

// Each step's column and the parts of it that are set again and again,
// found once as the panel is built: { column, led, switches, note, means }
const stepParts = [];

// Sets the low three mode switches together, as the rotary switch does
function setEngine(engine) {
    post({ mode: (state.mode & ~ENGINE_MASK) | engine });
}

function buildPanel() {
    for (let step = 0; step < STEP_COUNT; step++) {
        const column = document.createElement('div');
        const isGroupStart = (step % STEPS_PER_ROW) === STEPS_PER_GROUP;

        if ((step % STEPS_PER_ROW) === 0) {
            el('steps').insertAdjacentHTML('beforeend', '<div class="row-caption dim"></div>');
        }
        column.className = isGroupStart ? 'step group' : 'step';
        column.innerHTML = `<span class="dim num">${step}</span><span class="led"></span>`;
        for (let bit = NOTE_BITS - 1; bit >= 0; bit--) {
            const button = document.createElement('button');

            button.className = 'switch';
            button.textContent = String(1 << bit);
            button.dataset.bit = String(bit);
            button.addEventListener('click', () => {
                const notes = state.notes.slice();

                notes[step] ^= (1 << bit);
                post({ notes });
            });
            column.append(button);
        }
        column.insertAdjacentHTML('beforeend', '<span class="num note"></span>'
            + '<span class="num dim means"></span><span class="num dim means"></span>');
        el('steps').append(column);
        stepParts.push({
            column,
            led: column.querySelector('.led'),
            switches: [...column.querySelectorAll('.switch')],
            note: column.querySelector('.note'),
            means: [...column.querySelectorAll('.means')],
        });
    }
    // Mode switches 7 to 3, each a toggle with its number under it
    for (let bit = MODE_BITS - 1; bit >= ENGINE_BITS; bit--) {
        const cell = document.createElement('span');
        const button = document.createElement('button');

        cell.className = 'lever-cell dim num';
        button.className = 'switch lever';
        button.dataset.bit = String(bit);
        button.setAttribute('aria-label', `Mode switch ${bit}`);
        button.addEventListener('click', () => post({ mode: state.mode ^ (1 << bit) }));
        cell.append(button, String(bit));
        el('modes-spare').append(cell);
    }
    buildKnob('engine-knob', ENGINE_NAMES, engineNumber, setEngine);
    buildKnob('selector-knob', SELECTOR_KNOB_NAMES, () => state.selector,
        (selector) => post({ selector }));
}

// The number the low three mode switches make
function engineNumber() {
    return state.mode & ENGINE_MASK;
}

// The engine whose terms the panel is read in: the plain one in calibration
// mode, and for a number with no engine of its own
function panelEngine() {
    const engine = engineNumber();

    return (state.calibrating || (engine >= ENGINE_NAMES.length)) ? ENGINE_PLAIN : engine;
}

// Whether the panel is to be read as the stage engine reads it
function isStagePanel() {
    return panelEngine() === ENGINE_STAGE;
}

function isModeOn(bit) {
    return ((state.mode >> bit) & 1) === 1;
}

// The number a pair of mode switches makes, the lower of them at `shift`
function modePair(shift) {
    return (state.mode >> shift) & TWO_SWITCH_MASK;
}

function onOrOff(bit) {
    return isModeOn(bit) ? 'on' : 'off';
}

function everySteps(steps) {
    return (steps === 1) ? 'every step' : `every ${steps} steps`;
}

// How many beats the Euclidean engine puts in its bar, 1 to 16
function euclidBeats() {
    return ((state.mode >> EUCLID_BEATS_SHIFT) & EUCLID_BEATS_MASK) + 1;
}

// Whether a step of the bar is a beat. The bar and the pattern start
// together and are both sixteen long here, so a step keeps its place in it
function isEuclidBeat(step) {
    return ((step * euclidBeats()) % EUCLID_BAR_STEPS) < euclidBeats();
}

// What mode switches 7 to 3 mean to an engine and how they stand, highest
// first: a list of fields, each the switches read together, what they are
// called and what they say just now. A field called NO_USE has no use
function modeMeanings(engine) {
    const field = (switches, name, value) => ({ switches, name, value });

    switch (engine) {
        case ENGINE_KLEE: {
            const chance = modePair(KLEE_CHANCE_SHIFT);

            return [
                field([7, 6], 'chance', (chance === 0) ? 'off' : `${chance} in ${KLEE_CHANCE_OF}`),
                field([5], 'merge', onOrOff(KLEE_MERGE_BIT)),
                field([4], 'invert', onOrOff(KLEE_INVERT_BIT)),
                // At the step after it is moved either way
                field([3], 'load', 'any move'),
            ];
        }
        case ENGINE_CHANCE: {
            const skip = modePair(CHANCE_SKIP_SHIFT);

            return [
                field([7, 6], NO_USE, ''),
                field([5], 'shuffle', onOrOff(CHANCE_SHUFFLE_BIT)),
                field([4, 3], 'skip', (skip === 0) ? 'off' : `${skip} in ${CHANCE_SKIP_OF}`),
            ];
        }
        case ENGINE_CARTESIAN:
            return [
                field([7], NO_USE, ''),
                field([6, 5], 'y moves', everySteps(GRID_SIDE - modePair(GRID_Y_SHIFT))),
                field([4, 3], 'x moves', everySteps(modePair(GRID_X_SHIFT) + 1)),
            ];
        case ENGINE_EUCLID:
            return [
                field([7], NO_USE, ''),
                field([6, 5, 4, 3], 'beats', `${euclidBeats()} in a bar of ${EUCLID_BAR_STEPS}`),
            ];
        default:
            return [field([7, 6, 5, 4, 3], NO_USE, 'on this engine')];
    }
}

// Names mode switches 7 to 3 for the engine chosen: a legend over them, a
// box to a field and as wide as its switches, and the tip each one shows.
// A switch with no use is dimmed
function showModeMeanings() {
    const legend = el('mode-legend');

    legend.textContent = '';
    for (const { switches, name, value } of modeMeanings(panelEngine())) {
        const box = document.createElement('div');
        const title = document.createElement('b');
        const isUnused = name === NO_USE;

        box.className = isUnused ? 'field unused' : 'field num';
        box.style.gridColumn = `span ${switches.length}`;
        title.textContent = name;
        box.append(title, value);
        legend.append(box);
        for (const bit of switches) {
            const button = el('modes-spare').querySelector(`[data-bit="${bit}"]`);

            button.title = `Mode switch ${bit}: ${name} ${value}`.trim();
            button.parentElement.classList.toggle('unused', isUnused);
        }
    }
}

// Turns the engine's rotary switch to where the low three mode switches
// stand, and says so beside it with the code the switch is putting out
function showKnob() {
    const engine = engineNumber();
    const name = (engine < ENGINE_NAMES.length)
        ? ENGINE_NAMES[engine] : `plain (no engine ${engine} yet)`;

    el('engine').textContent = `${engine}, ${name} `
        + `(code ${engine.toString(2).padStart(ENGINE_BITS, '0')})`;
    turnKnob('engine-knob', engine, name);
}

function showModes() {
    const panelClass = ENGINE_CLASSES[panelEngine()];
    const captions = ROW_CAPTIONS[panelEngine()] ?? ['', ''];

    for (const button of el('modes-spare').querySelectorAll('.switch')) {
        const isOn = isModeOn(Number(button.dataset.bit));

        button.classList.toggle('on', isOn);
        button.setAttribute('aria-pressed', String(isOn));
    }
    showKnob();
    for (const engineClass of ENGINE_CLASSES.filter((each) => each !== '')) {
        el('steps').classList.toggle(engineClass, engineClass === panelClass);
    }
    el('steps').classList.toggle('terms', panelClass !== '');
    el('steps').querySelectorAll('.row-caption').forEach((caption, row) => {
        caption.textContent = captions[row];
    });
    for (const help of document.querySelectorAll('.engine-help')) {
        help.hidden = Number(help.dataset.engine) !== panelEngine();
    }
    showModeMeanings();
}

// The two lines under a step that say what it is to the engine chosen, top
// then bottom; both empty for an engine that reads a step as the plain one
// does. Stage: which stage's note, or a control's length (which counts
// while the stage plays) and gate mode. Klee: the pattern switch and the
// value. Cartesian: the column and row. Euclidean: whether it is a beat
function stepTerms(step, isPlaying) {
    const note = state.notes[step];

    switch (panelEngine()) {
        case ENGINE_STAGE:
            return (step < STAGE_COUNT) ? ['', `stage ${step}`] : [
                stageLengthText(step - STAGE_COUNT, isPlaying),
                STAGE_MODES[note >> STAGE_MODE_SHIFT],
            ];
        case ENGINE_KLEE:
            return [((note & KLEE_PATTERN_SWITCH) !== 0) ? 'pat on' : 'pat off',
                `value ${note & KLEE_VALUE_MASK}`];
        case ENGINE_CARTESIAN:
            return ['', `x ${step % GRID_SIDE}, y ${Math.floor(step / GRID_SIDE)}`];
        case ENGINE_EUCLID:
            return ['', isEuclidBeat(step) ? 'beat' : '–'];
        default:
            return ['', ''];
    }
}

// What a step's switches play, as the firmware logs it: a pitch one below
// the value, or a rest. Nothing for the Klee engine, where a note is a sum
function noteText(note) {
    if (panelEngine() === ENGINE_KLEE) {
        return '';
    }
    return (note === 0) ? 'rest' : String(note - 1);
}

function showPanel() {
    const volts = ((state.tempo + 0.5) * ADC_REFERENCE_VOLTS) / ADC_STEPS;

    state.notes.forEach((note, step) => {
        const parts = stepParts[step];

        for (const button of parts.switches) {
            button.classList.toggle('on', ((note >> Number(button.dataset.bit)) & 1) === 1);
        }
        parts.note.textContent = noteText(note);
        parts.column.classList.toggle('control', isStagePanel() && (step >= STAGE_COUNT));
    });
    showModes();
    if (document.activeElement !== el('tempo')) {
        el('tempo').value = String(state.tempo);
    }
    el('tempo-reading').textContent = `${state.tempo} (${volts.toFixed(2)} V)`;
    el('tempo-means').textContent = tempoMeaning();
    el('selector').textContent = SELECTOR_NAMES[state.selector];
    turnKnob('selector-knob', state.selector, SELECTOR_NAMES[state.selector]);
    if (document.activeElement !== el('lfo')) {
        el('lfo').value = String(state.lfo);
    }
    el('lfo-reading').textContent = lfoMeaning();
    if (document.activeElement !== el('rate')) {
        el('rate').value = String(state.rate);
    }
    el('rate-reading').textContent = rateMeaning();
    el('run').classList.toggle('down', state.runDown);
    el('profile').classList.toggle('down', state.profileDown);
    el('clock-on').checked = state.clockOn;
    el('clock-pulse').disabled = state.clockOn; // The jack is already taken
    el('clock-pattern').value = state.clockPattern;
    if (document.activeElement !== el('clock-ms')) {
        el('clock-ms').value = String(state.clockMs);
        showClockRate(state.clockMs);
    }
}

// What letting the profile button go would ask for just now, from how
// long the chip has seen it down; nothing while it is up
function profileMeaning(heldMs) {
    if (heldMs === null) {
        return '';
    }
    const meaning = PROFILE_HOLDS.find(([fromMs]) => heldMs >= fromMs)[1];

    return `${(heldMs / 1000).toFixed(1)} s: letting go ${meaning} profile `
        + `${state.selector}`;
}

// What the rate pot makes of the board's own LFO, as the firmware works
// it out: the reading over 4, squared, over 48, in 65536ths of a cycle a
// millisecond
function rateMeaning() {
    const step = Math.floor((Math.floor(state.rate / RATE_PER_LEVEL) ** 2) / RATE_DIVISOR);

    if (step === 0) {
        return `${state.rate}: off`;
    }

    const hertz = (step * 1000) / RATE_CYCLE;

    return (hertz < 1)
        ? `${state.rate}: a cycle every ${(1 / hertz).toFixed(1)} s`
        : `${state.rate}: ${hertz.toFixed(1)} Hz`;
}

// Where the LFO input stands, as the firmware takes it: nothing within 16
// of the centre, and the rest as a part of its swing either way
function lfoMeaning() {
    const swing = state.lfo - LFO_CENTRE;
    const percent = Math.round((Math.abs(swing) / LFO_CENTRE) * 100);

    if (Math.abs(swing) < LFO_DEAD_ZONE) {
        return `${state.lfo}: at its centre, moving nothing`;
    }
    return `${state.lfo}: ${percent}% ${swing > 0 ? 'above' : 'below'} its centre`;
}

// What the pot does just now, from where the selector is and the clock the
// pattern is on. The firmware takes a new ratio at its next sync edge. The
// tempo given is where the pot stands, which is the tempo only once the pot
// has met it
function tempoMeaning() {
    const part = Math.min(RATIOS.length - 1, Math.floor(state.tempo / READING_PER_RATIO));

    if (state.calibrating) {
        return 'The pot trims the calibration point';
    }
    if (state.selector !== 0) {
        return `The pot sets: ${SELECTOR_NAMES[state.selector]}, once it has met the amount `
            + 'held (the level light)';
    }
    if (state.transport !== TRANSPORT_EXTERNAL) {
        return `The pot is at ${BPM_MIN + Math.floor(state.tempo / READING_PER_BPM)} BPM`;
    }
    return `The pot is at ${RATIOS[part][0]}: ${RATIOS[part][1]}`;
}

// What the clock source's rate comes to with its pattern: a swung clock
// keeps the rate on average, and a random one has an edge on half its slots
function showClockRate(periodMs) {
    const isRandom = RANDOM_CLOCK_PATTERNS.includes(state.clockPattern);
    const edgeMs = isRandom ? (periodMs * RANDOM_SLOTS_PER_EDGE) : periodMs;
    const bpm = MS_PER_MINUTE / (edgeMs * STEPS_PER_BEAT);
    let text = `${periodMs} ms between edges`;

    if (isRandom) {
        text = `slots of ${periodMs} ms, an edge every ${edgeMs} ms on average`;
    } else if (state.clockPattern !== 'steady') {
        text += ' on average';
    } else {
        // A steady clock: the rate is the gap
    }
    el('clock-rate').textContent = `${text} (${bpm.toFixed(1)} BPM at ×1)`;
}

// The transport as the server has it: from the log's last `Transport = N`
// if there has been one since the controls last changed it, and otherwise
// worked out from the presses of the run button and the clock edges
function showTransport() {
    let text = 'Calibration mode: nothing is sequenced, so these do nothing';
    let from = '';

    if (!state.calibrating) {
        text = TRANSPORT_NAMES[state.transport];
        if ((state.transport === TRANSPORT_EXTERNAL) && !state.clockOn) {
            text += ', waiting for an edge: the clock source is off';
        }
        if (state.transportFrom === 'log') {
            from = '(as the log says)';
        } else if (state.transportFrom === 'midi') {
            from = '(as the MIDI Start or Stop says)';
        } else if (!state.hasLog) {
            from = `(worked out from the controls: the ${state.config} image has no log)`;
        } else {
            from = '(worked out from the controls)';
        }
        if (state.transport === TRANSPORT_STOPPED) {
            // The next clock pulse is not a step on from the last
            lastClockRiseMs = null;
            el('bpm').textContent = '';
        }
    }
    el('transport').textContent = text;
    el('transport-from').textContent = from;
}

// What a stage's control says of its length: how many steps, or which of
// them is playing
function stageLengthText(stage, isPlaying) {
    const steps = (state.notes[STAGE_COUNT + stage] & STAGE_LENGTH_MASK) + 1;

    if (isPlaying && (state.stageSteps > 0)) {
        return `${state.stageStep} of ${state.stageSteps}`;
    }
    return (steps === 1) ? '1 step' : `${steps} steps`;
}

// What is said beside the mode switches of the step counted, where an
// outline cannot say it: on the Klee engine the step is how far the
// register has moved, and the server sends -1 for a step it cannot name
function placeText(step) {
    if (state.calibrating || (state.transport === TRANSPORT_STOPPED)) {
        return '';
    }
    if (panelEngine() === ENGINE_KLEE) {
        return (step < 0) ? '' : `register moved ${step} since its load`;
    }
    if ((panelEngine() === ENGINE_CHANCE) && isModeOn(CHANCE_SHUFFLE_BIT) && (step < 0)) {
        return 'step not known: shuffled, and this image has no log';
    }
    return '';
}

// Outlines the step counted by the server. None is outlined while stopped:
// the pattern will start again from step 0. Nor when the server sends -1,
// nor on the Klee engine, where the count is not a step; see placeText().
// On the stage engine the step is a stage, outlined with its control
function showStep() {
    const isCounted = (state.transport !== TRANSPORT_STOPPED)
        && (panelEngine() !== ENGINE_KLEE);
    const playing = isCounted ? state.step : -1;
    const isStage = isStagePanel();

    el('place').textContent = placeText(state.step);

    for (let step = 0; step < STEP_COUNT; step++) {
        const { column, means: [top, bottom] } = stepParts[step];
        const isControl = isStage && (step >= STAGE_COUNT);
        const isPlaying = (step === playing)
            || (isControl && ((step - STAGE_COUNT) === playing));

        column.classList.toggle('playing', isPlaying);
        [top.textContent, bottom.textContent] = stepTerms(step, isPlaying);
    }
}

// The sixteen step lights, as the chip last latched them: bit n of the
// value is the light of step n. Nothing else on the page lights them
function showLeds() {
    stepParts.forEach(({ led }, step) => {
        led.classList.toggle('lit', ((state.leds >> step) & 1) === 1);
    });
}
