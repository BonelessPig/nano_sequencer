'use strict';
// The gear patched to the board's jacks, and the scope on its signals
// ---- Gear: a VCO on the pitch CV, an envelope on the gate, and a VCA ----

// None of this is the board, and the server knows nothing of it: it is
// what the board's outputs are plugged into, made in the browser from the
// same `cv` and `gate` events the Outputs section shows

// A few seconds of white noise, to go round and round
function noiseBuffer(context) {
    const buffer = context.createBuffer(
        1, context.sampleRate * NOISE_SECONDS, context.sampleRate);
    const samples = buffer.getChannelData(0);

    for (let i = 0; i < samples.length; i++) {
        samples[i] = (Math.random() * 2) - 1;
    }
    return buffer;
}

// A probe on one signal, holding the last `size` samples of it
function probe(context, signal, size) {
    const analyser = context.createAnalyser();

    analyser.fftSize = size;
    signal.connect(analyser);
    return { analyser, samples: new Float32Array(size) };
}

// The patch: the oscillator and the noise each through a switch to the VCO's
// output, that through the VCA to the audio output, and a steady 1 through
// the envelope to the VCA's CV. The volume is the listener's, after the
// audio output, so the scope does not show it
function buildGear() {
    const context = new AudioContext();
    const oscillator = context.createOscillator();
    const noise = context.createBufferSource();
    const oscillatorOn = context.createGain();
    const noiseOn = context.createGain();
    const vcoOut = context.createGain();
    const one = context.createConstantSource();
    const envelope = context.createGain();
    const vca = context.createGain();
    const volume = context.createGain();

    noise.buffer = noiseBuffer(context);
    noise.loop = true;
    envelope.gain.value = 0;
    vca.gain.value = 0;
    oscillator.connect(oscillatorOn).connect(vcoOut);
    noise.connect(noiseOn).connect(vcoOut);
    vcoOut.connect(vca).connect(volume).connect(context.destination);
    one.connect(envelope).connect(vca.gain);
    oscillator.start();
    noise.start();
    one.start();
    return {
        context, oscillator, oscillatorOn, noiseOn, envelope, volume,
        vcoProbe: probe(context, vcoOut, SCOPE_PROBE_SAMPLES),
        outProbe: probe(context, vca, SCOPE_PROBE_SAMPLES),
        envelopeProbe: probe(context, envelope, ENVELOPE_PROBE_SAMPLES),
        // Each a list of [ms, value], as the trace keeps its signals: the
        // envelope's level, and the VCO's pitch in octaves over VCO_BASE_HZ
        envelopePoints: [],
        pitchPoints: [],
        sampledMs: 0,
    };
}

const lastValue = (points) => points[points.length - 1][1];

// Adds a value at the present chip time to one of the gear's lists, which
// is emptied first if chip time has begun again, as at a power cycle
function addPoint(points, value) {
    if ((points.length > 0) && (nowMs < points[points.length - 1][0])) {
        points.length = 0;
    }
    points.push([nowMs, value]);
}

// The pitch CV at the VCO: an octave a volt, from where the tune knob is
function gearPitch(millivolts) {
    if (gear !== null) {
        const octaves = (millivolts / 1000)
            + (Number(el('vco-tune').value) / SEMITONES_PER_OCTAVE);

        addPoint(gear.pitchPoints, octaves);
        gear.oscillator.frequency.setValueAtTime(
            VCO_BASE_HZ * (2 ** octaves), gear.context.currentTime);
    }
}

// The gate at the envelope: up at the attack's rate while it is high, down
// at the release's once it is low
function gearGate(isHigh) {
    if (gear !== null) {
        const ms = Number(el(isHigh ? 'env-attack' : 'env-release').value);

        gear.envelope.gain.setTargetAtTime(isHigh ? 1 : 0,
            gear.context.currentTime, ms / (MS_PER_S * ENVELOPE_SETTLES));
    }
}

// The gear's knobs, worded, and put on the gear if it is on
function applyGear() {
    const wave = el('vco-wave').value;
    const tune = Number(el('vco-tune').value);

    el('vco-tune-reading').textContent = `${(tune > 0) ? '+' : ''}${tune} semitones`;
    el('env-attack-reading').textContent = `${el('env-attack').value} ms`;
    el('env-release-reading').textContent = `${el('env-release').value} ms`;
    el('gear-power').textContent = `Gear: ${(gear === null) ? 'off' : 'on'}`;
    if (gear !== null) {
        const isNoise = wave === VCO_NOISE;

        if (!isNoise) {
            gear.oscillator.type = wave;
        }
        gear.oscillatorOn.gain.value = isNoise ? 0 : 1;
        gear.noiseOn.gain.value = isNoise ? 1 : 0;
        gear.volume.gain.value = Number(el('volume').value) / VOLUME_STEPS;
        gearPitch(lastValue(trace.cv));
    }
}

function toggleGear() {
    if (state === null) {
        return; // Nothing to play until the server has answered
    }
    if (gear === null) {
        gear = buildGear();
        gearGate(lastValue(trace.gate) === 1);
    } else {
        gear.context.close();
        gear = null;
    }
    applyGear();
}

// ---- Scope ----

// Where a rising signal first crosses zero, so a wave stands still from
// one frame to the next; 0 if it never does, as noise may not
function triggerAt(samples, last) {
    for (let i = 1; i <= last; i++) {
        if ((samples[i - 1] < 0) && (samples[i] >= 0)) {
            return i;
        }
    }
    return 0;
}

// Draws SCOPE_SAMPLES of a probe from `start`, -1 to 1 over the band
function drawWave(context, samples, start, colour, top, height, width) {
    context.strokeStyle = colour;
    context.beginPath();
    for (let i = 0; i < SCOPE_SAMPLES; i++) {
        const x = width * (i / (SCOPE_SAMPLES - 1));
        const y = top + (height * (1 - ((samples[start + i] + 1) / 2)));

        if (i === 0) {
            context.moveTo(x, y);
        } else {
            context.lineTo(x, y);
        }
    }
    context.stroke();
}

// Takes each probe's samples, and adds where the envelope stands to its
// points once for every slice of chip time, so it lines up with the trace
function readProbes() {
    const { vcoProbe, outProbe, envelopeProbe } = gear;

    for (const one of [vcoProbe, outProbe, envelopeProbe]) {
        one.analyser.getFloatTimeDomainData(one.samples);
    }
    if (nowMs !== gear.sampledMs) {
        addPoint(gear.envelopePoints,
            envelopeProbe.samples[ENVELOPE_PROBE_SAMPLES - 1]);
        gear.sampledMs = nowMs;
    }
}

// The VCO's frequency over the trace's window, in a band whose height is a
// whole number of octaves, so a semitone is the same rise wherever it is.
// Returns the band's label
function drawPitch(context, colour, top, height, width) {
    let low = 0;
    let high = 0;

    if (el('vco-wave').value === VCO_NOISE) {
        return 'VCO frequency: white noise has none';
    }
    trim(gear.pitchPoints);
    low = Math.floor(Math.min(...gear.pitchPoints.map((point) => point[1])));
    high = Math.max(low + 1,
        Math.ceil(Math.max(...gear.pitchPoints.map((point) => point[1]))));
    drawSignal(context, gear.pitchPoints.map(([ms, octaves]) => [ms, octaves - low]),
        colour, top, height, high - low, width);
    return `VCO frequency, ${(VCO_BASE_HZ * (2 ** low)).toFixed(0)} to `
        + `${(VCO_BASE_HZ * (2 ** high)).toFixed(0)} Hz, `
        + `${TRACE_WINDOW_MS / MS_PER_S} s`;
}

// Four bands: the VCO's output and the audio output as waves a few
// milliseconds long, and between them the VCO's frequency and the envelope
// over the trace's window
function drawScope() {
    const { context, width, height, colour } = openCanvas(el('scope'));
    const row = height / SCOPE_ROWS;
    const bandHeight = row - SCOPE_LABEL_PX - SCOPE_GAP_PX;
    const top = (n) => (row * n) + SCOPE_LABEL_PX;
    const label = (text, n) => context.fillText(text, 4, (row * n) + 12);

    context.fillStyle = colour('--dim');
    if (gear === null) {
        label('The gear is off', 0);
    } else {
        const spanMs = (SCOPE_SAMPLES * MS_PER_S) / gear.context.sampleRate;
        let start = 0;

        readProbes();
        start = triggerAt(gear.vcoProbe.samples, SCOPE_PROBE_SAMPLES - SCOPE_SAMPLES);
        label(`VCO out, ${spanMs.toFixed(0)} ms`, 0);
        label(drawPitch(context, colour('--cv'), top(1), bandHeight, width), 1);
        label(`Envelope into the VCA's CV, ${TRACE_WINDOW_MS / MS_PER_S} s`, 2);
        label(`Audio out, after the VCA, ${spanMs.toFixed(0)} ms`, 3);
        drawWave(context, gear.vcoProbe.samples, start, colour('--cv'),
            top(0), bandHeight, width);
        drawSignal(context, gear.envelopePoints, colour('--gate'),
            top(2), bandHeight, 1, width);
        drawWave(context, gear.outProbe.samples, start, colour('--on'),
            top(3), bandHeight, width);
    }
    requestAnimationFrame(drawScope);
}
