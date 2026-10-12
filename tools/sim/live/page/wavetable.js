'use strict';
// The wavetables' waves worked out, and the picture of the one chosen
// ---- Wavetables ----

// Each table's waves, and the lines its picture is drawn with, worked out
// the first time they are asked for
const tablesMade = {};
const picturesMade = {};

// A wave from its harmonics: their cosine and sine amounts as the
// oscillator takes them, and one cycle as heights. All are brought to a
// peak of 1, so a mix of two waves is the same mix drawn and played
function makeWave(wave) {
    const real = new Float32Array(WAVETABLE_HARMONICS + 1);
    const imag = new Float32Array(WAVETABLE_HARMONICS + 1);
    const shape = [];
    let peak = 0;

    for (let n = 1; n <= WAVETABLE_HARMONICS; n++) {
        [real[n], imag[n]] = wave.part(n);
    }
    for (let j = 0; j <= WAVE_PICTURE_POINTS; j++) {
        let sum = 0;

        for (let n = 1; n <= WAVETABLE_HARMONICS; n++) {
            const angle = (2 * Math.PI * n * j) / WAVE_PICTURE_POINTS;

            sum += (real[n] * Math.cos(angle)) + (imag[n] * Math.sin(angle));
        }
        shape.push(sum);
        peak = Math.max(peak, Math.abs(sum));
    }
    return { real: real.map((value) => value / peak),
        imag: imag.map((value) => value / peak),
        shape: shape.map((value) => value / peak) };
}

// The waves of a table, each as { real, imag, shape }
function tableWaves(name) {
    tablesMade[name] ??= WAVETABLES[name].map(makeWave);
    return tablesMade[name];
}

// One cycle of the wave at a place in a table: the two it is between, mixed
function shapeAt(waves, place) {
    const { low, part } = between(place, waves.length);

    return waves[low].shape.map(
        (value, j) => ((1 - part) * value) + (part * waves[low + 1].shape[j]));
}

// ---- Wavetable picture ----

// It is drawn from the tables and the pots alone, so it shows with the
// gear off too

// A colour part of the way from one to another, each given as #rrggbb
function mixColour(from, to, part) {
    const channel = (colour, i) => parseInt(colour.slice(1 + (2 * i), 3 + (2 * i)), 16);

    return `rgb(${[0, 1, 2].map((i) => Math.round(channel(from, i)
        + (part * (channel(to, i) - channel(from, i))))).join(', ')})`;
}

// Where the picture's parts are on a canvas of this size: the front wave's
// left end and middle line, a wave's width and half height, and how far
// right and up each wave behind stands from the one before
function pictureBox(width, height, waves) {
    const half = height * PICTURE_WAVE_HEIGHT;

    return {
        left: width * PICTURE_LEFT,
        base: height - half - PICTURE_FOOT_PX,
        waveWidth: width * PICTURE_WAVE_WIDTH,
        half,
        stepX: (width * PICTURE_DEPTH_X) / (waves - 1),
        stepY: (height * PICTURE_DEPTH_Y) / (waves - 1),
        height,
    };
}

// Traces one wave, `depth` waves back from the front. Returns its left end
function traceWave(context, shape, depth, box) {
    const x0 = box.left + (depth * box.stepX);
    const y0 = box.base - (depth * box.stepY);

    context.beginPath();
    shape.forEach((value, j) => {
        const x = x0 + ((box.waveWidth * j) / (shape.length - 1));
        const y = y0 - (value * box.half);

        if (j === 0) {
            context.moveTo(x, y);
        } else {
            context.lineTo(x, y);
        }
    });
    return x0;
}

// Draws one line of the table, with everything under it painted out, so it
// hides what stands behind it
function drawTableLine(context, shape, depth, box, colour, card) {
    const x0 = traceWave(context, shape, depth, box);

    context.lineTo(x0 + box.waveWidth, box.height);
    context.lineTo(x0, box.height);
    context.fillStyle = card;
    context.fill();
    traceWave(context, shape, depth, box);
    context.strokeStyle = colour;
    context.stroke();
}

// The picture is drawn as a solid seen from above its front: everything in
// it is a line at a depth, drawn from the back to the front, and a line of
// the table paints out all that is under it. So a wave being played is
// hidden wherever a nearer part of the table stands in front of it. Each
// of these gives its lines as { depth, over, draw }: `over` puts lines at
// one depth in order, the table's first

// The lines a table's picture is drawn with, each as { depth, shape }: its
// waves and as many between each two as bring it near PICTURE_LINES_ABOUT.
// They are the same every frame, so they are kept
function pictureLines(name) {
    const waves = tableWaves(name);
    const perWave = Math.max(1, Math.round(PICTURE_LINES_ABOUT / (waves.length - 1)));

    picturesMade[name] ??= Array.from({ length: ((waves.length - 1) * perWave) + 1 },
        (unused, i) => ({ depth: i / perWave, shape: shapeAt(waves, i / perWave) }));
    return picturesMade[name];
}

// The table itself, the back the darkest and the front the lightest
function tableLines(context, name, box, colour) {
    const lines = pictureLines(name);
    const front = colour('--wave-front');
    const back = colour('--wave-back');
    const card = colour('--card');

    return lines.map(({ depth, shape }, i) => ({
        depth,
        over: 0,
        draw: () => {
            context.lineWidth = PICTURE_TABLE_WIDTH;
            drawTableLine(context, shape, depth, box,
                mixColour(front, back, i / (lines.length - 1)), card);
        },
    }));
}

// The scan: a short bright length of a wave being played that goes along it
// from left to right and round again, faster the higher the pitch, and as
// bright as the envelope stands. The shape is cut to the length and drawn
// in place
function drawScan(context, shape, depth, box, colour, lineWidth, alpha) {
    const head = gear.scan * WAVE_PICTURE_POINTS;
    const first = Math.max(0, Math.floor(head - (PICTURE_SCAN_TAIL * WAVE_PICTURE_POINTS)));
    const last = Math.max(first + 1, Math.ceil(head));
    const cut = shape.slice(first, last + 1);
    const cutBox = { ...box,
        left: box.left + ((box.waveWidth * first) / WAVE_PICTURE_POINTS),
        waveWidth: (box.waveWidth * (last - first)) / WAVE_PICTURE_POINTS };

    context.save();
    context.globalAlpha = gear.envelopeLevel * alpha;
    context.lineWidth = lineWidth;
    context.lineCap = 'round';
    context.strokeStyle = colour('--scan');
    context.shadowColor = colour('--on');
    context.shadowBlur = PICTURE_SCAN_GLOW_PX;
    traceWave(context, cut, depth, cutBox);
    context.stroke();
    context.restore();
}

// One wave being played, at its place in the table: its line, and with the
// gear on, which is where there is an envelope, the scan along it
function voiceLine(context, waves, box, colour, place, voice) {
    return {
        depth: place,
        over: voice.over,
        draw: () => {
            const shape = shapeAt(waves, place);

            context.save();
            context.globalAlpha = voice.alpha;
            context.lineWidth = voice.width;
            context.strokeStyle = colour('--on');
            traceWave(context, shape, place, box);
            context.stroke();
            context.restore();
            if (gear !== null) {
                drawScan(context, shape, place, box, colour, voice.scanWidth, voice.alpha);
            }
        },
    };
}

// The waves being played: each of the chorus's voices that is there at
// all, as solid as it is there, and over them the wave of the VCO's first
// voice, which is the one the position pot is at
function voiceLines(context, waves, box, colour) {
    const places = voicePlaces(waves.length);
    const presences = voicePresences();
    const lines = gearVoices.slice(1)
        .filter((k) => presences[k] > 0)
        .map((k) => voiceLine(context, waves, box, colour, places[k], {
            over: 1, alpha: PICTURE_VOICE_ALPHA * presences[k],
            width: PICTURE_VOICE_WIDTH, scanWidth: PICTURE_VOICE_SCAN_WIDTH }));

    lines.push(voiceLine(context, waves, box, colour, places[0], {
        over: 2, alpha: 1, width: PICTURE_CHOSEN_WIDTH, scanWidth: PICTURE_SCAN_WIDTH }));
    return lines;
}

function drawWavetable() {
    const name = el('vco-wave').value;
    const canvas = el('wavetable');

    canvas.hidden = WAVETABLES[name] === undefined;
    if (!canvas.hidden) {
        const { context, width, height, colour } = openCanvas(canvas);
        const waves = tableWaves(name);
        const last = waves.length - 1;
        const box = pictureBox(width, height, waves.length);
        const lines = [...tableLines(context, name, box, colour),
            ...voiceLines(context, waves, box, colour)];

        lines.sort((a, b) => (b.depth - a.depth) || (a.over - b.over));
        lines.forEach((line) => line.draw());
        context.fillStyle = colour('--dim');
        context.fillText(`Front: ${WAVETABLES[name][0].name}`,
            box.left, box.base + box.half + 13);
        context.fillText(`Back: ${WAVETABLES[name][last].name}`,
            box.left + (last * box.stepX), box.base - (last * box.stepY) - box.half - 5);
    }
}
