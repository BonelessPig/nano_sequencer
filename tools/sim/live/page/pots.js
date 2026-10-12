'use strict';
// Every slider of the page drawn as a pot
// ---- Pots ----

// The slider itself stays, out of sight inside its pot, and is still what
// holds the value: the page reads and sets it as before, a label still
// finds it, and with the pot clicked it has the focus, so the arrow keys
// turn it. Dragging up or down turns the pot, and raises the slider's own
// `input` events on the way and `change` when let go
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
function dragPot(input, from, y) {
    const min = Number(input.min);
    const max = Number(input.max);
    const moved = ((from.y - y) / POT_DRAG_PX) * (max - min);
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
    pot.addEventListener('pointerdown', (event) => {
        event.preventDefault();
        input.focus();
        pot.setPointerCapture(event.pointerId);
        from = { y: event.clientY, value: Number(input.value) };
    });
    pot.addEventListener('pointermove', (event) => {
        if (from !== null) {
            dragPot(input, from, event.clientY);
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
