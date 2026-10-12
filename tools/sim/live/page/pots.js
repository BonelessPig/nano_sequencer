'use strict';
// The page's knobs: its two rotary switches, and every slider drawn as a pot
// ---- Rotary switches ----

// Where a position of the rotary switch is on its picture, at a distance
// from the middle
function knobPoint(position, radius) {
    const radians = ((KNOB_FIRST_DEG + (position * KNOB_STEP_DEG)) * Math.PI) / 180;

    return [KNOB_X + (radius * Math.sin(radians)), KNOB_Y - (radius * Math.cos(radians))];
}

// Draws a rotary switch of eight positions: a dot and a name at each, a
// bare number where `names` has none, and the knob with its pointer. A
// position is chosen by clicking its name, by clicking the knob for the
// next one round, or with the arrow keys. `current` says where the switch
// stands and `set` posts a new position
function buildKnob(id, names, current, set) {
    const knob = el(id);
    let marks = '';

    for (let position = 0; position < KNOB_POSITIONS; position++) {
        const [dotX, dotY] = knobPoint(position, KNOB_DOT_RADIUS);
        const [textX, textY] = knobPoint(position, KNOB_LABEL_RADIUS);

        marks += `<g class="detent" data-position="${position}">`
            + `<circle cx="${dotX.toFixed(1)}" cy="${dotY.toFixed(1)}" r="3"/>`
            + `<text x="${textX.toFixed(1)}" y="${textY.toFixed(1)}"`
            + ` text-anchor="${(textX < KNOB_X) ? 'end' : 'start'}">`
            + `${names[position] ?? position}</text></g>`;
    }
    knob.innerHTML = `<svg viewBox="0 0 ${KNOB_WIDTH} ${KNOB_HEIGHT}" aria-hidden="true">`
        + `${marks}<circle class="knob-body" cx="${KNOB_X}" cy="${KNOB_Y}"`
        + ` r="${KNOB_RADIUS}"/><line class="knob-pointer" x1="${KNOB_X}" y1="${KNOB_Y}"`
        + ` x2="${KNOB_X}" y2="${KNOB_Y - KNOB_RADIUS + 4}"/></svg>`;
    for (const detent of knob.querySelectorAll('.detent')) {
        detent.addEventListener('click', () => set(Number(detent.dataset.position)));
    }
    knob.querySelector('.knob-body').addEventListener('click',
        () => set((current() + 1) % KNOB_POSITIONS));
    knob.addEventListener('keydown', (event) => {
        const turn = { ArrowRight: 1, ArrowUp: 1, ArrowLeft: -1, ArrowDown: -1 }[event.key];

        if (turn !== undefined) {
            event.preventDefault();
            set(Math.max(0, Math.min(KNOB_POSITIONS - 1, current() + turn)));
        }
    });
}

// Turns a rotary switch's picture to a position, which `name` words
function turnKnob(id, position, name) {
    const knob = el(id);
    const degrees = KNOB_FIRST_DEG + (position * KNOB_STEP_DEG);

    knob.setAttribute('aria-valuenow', String(position));
    knob.setAttribute('aria-valuetext', name);
    knob.querySelector('.knob-pointer')
        .setAttribute('transform', `rotate(${degrees} ${KNOB_X} ${KNOB_Y})`);
    for (const detent of knob.querySelectorAll('.detent')) {
        detent.classList.toggle('chosen', Number(detent.dataset.position) === position);
    }
}

// ---- Pots ----

// The slider itself stays, out of sight inside its pot, and is still what
// holds the value: the page reads and sets it as before, a label still
// finds it, and with the pot clicked it has the focus, so the arrow keys
// turn it. Dragging up or down turns the pot, and raises the slider's own
// `input` events on the way and `change` when let go. With Shift held a
// drag turns it a fifth as far, and a double click puts a pot that the page
// gave a value back to it; one whose value is the board's has nothing to
// go back to
const pots = [];    // Each { input, pointer, shown }: the value last drawn

// How far round a pot's pointer is for its slider's value, clockwise from up
function potDegrees(input) {
    const min = Number(input.min);
    const part = (Number(input.value) - min) / (Number(input.max) - min);

    return (part - 0.5) * POT_SWEEP_DEG;
}

// A dot at one end of a pot's travel: -1 the low end, 1 the high
function potEndDot(side) {
    const radians = (side * (POT_SWEEP_DEG / 2) * Math.PI) / 180;
    const x = POT_MIDDLE + (POT_DOT_RADIUS * Math.sin(radians));
    const y = POT_MIDDLE - (POT_DOT_RADIUS * Math.cos(radians));

    return `<circle class="pot-end" cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="1.5"/>`;
}

// Sets a slider as a drag has it, and says so as the slider would
function dragPot(input, from, y, isFine) {
    const min = Number(input.min);
    const max = Number(input.max);
    const moved = ((from.y - y) / (POT_DRAG_PX * (isFine ? POT_FINE_DRAG : 1))) * (max - min);
    const next = String(Math.round(Math.max(min, Math.min(max, from.value + moved))));

    if (next !== input.value) {
        input.value = next;
        input.dispatchEvent(new Event('input', { bubbles: true }));
    }
}

function buildPot(input) {
    const pot = document.createElement('span');
    let from = null;    // Where a drag began: its y and the value then

    pot.className = 'pot';
    pot.innerHTML = `<svg viewBox="0 0 ${POT_SIZE} ${POT_SIZE}" aria-hidden="true">`
        + `${potEndDot(-1)}${potEndDot(1)}<circle class="knob-body" cx="${POT_MIDDLE}"`
        + ` cy="${POT_MIDDLE}" r="${POT_RADIUS}"/><line class="knob-pointer"`
        + ` x1="${POT_MIDDLE}" y1="${POT_MIDDLE}" x2="${POT_MIDDLE}"`
        + ` y2="${POT_MIDDLE - POT_RADIUS + 3}"/></svg>`;
    input.replaceWith(pot);
    pot.append(input);
    pot.title = `Drag up or down, or use the arrow keys; hold Shift to go slowly${
        input.hasAttribute('value') ? '; double-click to put it back' : ''}`;
    pot.addEventListener('dblclick', () => {
        if (input.hasAttribute('value') && (input.value !== input.defaultValue)) {
            input.value = input.defaultValue;
            input.dispatchEvent(new Event('input', { bubbles: true }));
            input.dispatchEvent(new Event('change', { bubbles: true }));
        }
    });
    pot.addEventListener('pointerdown', (event) => {
        event.preventDefault();
        input.focus();
        pot.setPointerCapture(event.pointerId);
        from = { y: event.clientY, value: Number(input.value) };
    });
    pot.addEventListener('pointermove', (event) => {
        if (from !== null) {
            dragPot(input, from, event.clientY, event.shiftKey);
        }
    });
    for (const end of ['pointerup', 'pointercancel']) {
        pot.addEventListener(end, () => {
            if (from !== null) {
                from = null;
                input.dispatchEvent(new Event('change', { bubbles: true }));
            }
        });
    }
    pots.push({ input, pointer: pot.querySelector('.knob-pointer'), shown: null });
}

function buildPots() {
    document.querySelectorAll('input[type="range"]').forEach(buildPot);
}

// Turns each pot to where its slider stands. Looked at every frame, since
// a value set from the server's state raises no event to hear
function drawPots() {
    for (const pot of pots) {
        if (pot.input.value !== pot.shown) {
            pot.shown = pot.input.value;
            pot.pointer.setAttribute('transform',
                `rotate(${potDegrees(pot.input)} ${POT_MIDDLE} ${POT_MIDDLE})`);
        }
    }
    requestAnimationFrame(drawPots);
}
