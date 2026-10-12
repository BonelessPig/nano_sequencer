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

// The patch: the oscillators and the noise each through a switch to the
// VCO's output, that through the VCA to the audio output, and a steady 1
// through the envelope to the VCA's CV. The volume is the listener's, after
// the audio output, so the scope does not show it. The VCO is one
// oscillator and CHORUS_VOICES more, each through its own gain: the first
// plays alone until the chorus pot brings the rest in
function buildGear() {
    const context = new AudioContext();
    const oscillators = [];
    const gains = [];
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
    for (let k = 0; k <= CHORUS_VOICES; k++) {
        oscillators.push(context.createOscillator());
        gains.push(context.createGain());
        gains[k].gain.value = (k === 0) ? 1 : 0;
        oscillators[k].connect(gains[k]).connect(oscillatorOn);
    }
    oscillatorOn.connect(vcoOut);
    noise.connect(noiseOn).connect(vcoOut);
    vcoOut.connect(vca).connect(volume).connect(context.destination);
    one.connect(envelope).connect(vca.gain);
    oscillators.forEach((oscillator) => oscillator.start());
    noise.start();
    one.start();
    return {
        context, oscillators, gains, oscillatorOn, noiseOn, envelope, volume,
        vcoProbe: probe(context, vcoOut, SCOPE_PROBE_SAMPLES),
        outProbe: probe(context, vca, SCOPE_PROBE_SAMPLES),
        envelopeProbe: probe(context, envelope, ENVELOPE_PROBE_SAMPLES),
        // Each a list of [ms, value], as the trace keeps its signals: the
        // envelope's level, and the VCO's pitch in octaves over VCO_BASE_HZ
        envelopePoints: [],
        pitchPoints: [],
        sampledMs: 0,
        held: [],               // What wave each voice was last given
        envelopeLevel: 0,       // As last taken from the envelope's probe
        scan: 0,                // How far along the wave the picture's scan is
        scannedAt: null,        // The frame time it was last moved at
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

// The pitch CV at the VCO: an octave a volt, from where the tune knob is.
// The pitch is kept in octaves over VCO_BASE_HZ
function gearPitch(millivolts) {
    if (gear !== null) {
        const octaves = (millivolts / 1000)
            + (Number(el('vco-tune').value) / SEMITONES_PER_OCTAVE);
        const hertz = VCO_BASE_HZ * (2 ** octaves);

        addPoint(gear.pitchPoints, octaves);
        for (const one of gear.oscillators) {
            one.frequency.setValueAtTime(hertz, gear.context.currentTime);
        }
    }
}

const chorusAmount = () => Number(el('chorus').value) / CHORUS_STEPS;
const sweepAmount = () => Number(el('env-position').value) / SWEEP_STEPS;
const clamp = (value) => Math.max(0, Math.min(1, value));

// Where in a wavetable the VCO is, 0 its first wave and 1 its last: where
// the position pot is, and on from there by as much of the envelope as the
// envelope's position pot lets through. With the gear off there is no
// envelope
function tableCentre() {
    const level = (gear === null) ? 0 : gear.envelopeLevel;

    return Math.min(1, (Number(el('vco-position').value) / POSITION_STEPS)
        + (level * sweepAmount()));
}

// Brings the wavetable's picture up to this frame, at `time` in ms: the
// envelope's level from its probe, which is how bright the picture's scan
// is, and the scan moved on along the wave by the time since the last
// frame, the faster the higher the pitch. The gear has had a pitch since
// it was turned on
function followGear(time) {
    const rate = Math.min(PICTURE_SCAN_MAX_HZ,
        PICTURE_SCAN_HZ * (2 ** lastValue(gear.pitchPoints)));
    const wave = el('vco-wave').value;
    const level = clamp(gear.envelopeProbe.samples[ENVELOPE_PROBE_SAMPLES - 1]);
    const hasMoved = Math.abs(level - gear.envelopeLevel) >= ENVELOPE_STEP;

    gear.envelopeLevel = level;
    if (hasMoved && (sweepAmount() > 0) && (WAVETABLES[wave] !== undefined)) {
        // The envelope is carrying the VCO along the table
        applyVoices(wave);
        el('vco-position-reading').textContent = positionReading();
    }
    gear.scan = (gear.scan + (((time - (gear.scannedAt ?? time)) / MS_PER_S) * rate)) % 1;
    gear.scannedAt = time;
    drawWavetable();
}

// How much of each voice of the chorus is there, 0 to 1, at where the
// chorus pot is: the more of them the further up it is, their number going
// up exponentially, the last fully in at the pot's top. Voice 0, the
// VCO's first, is not the chorus's and is not to be read from this
function voicePresences() {
    const chorus = chorusAmount();

    return VOICE_COMES_IN.map((from) => clamp((chorus - from) / CHORUS_VOICE_FADE));
}

// Where each voice is in a table of this many waves, counted in waves from
// the first. Voice 0 is where the position pot is. The chorus's voices are
// there too with the chorus pot at 0, and go out from it as the pot comes
// up, until at the top they reach both ends of the table. They are not
// spread evenly over it: each one's even place is drawn in towards voice 0,
// so they stand close together near it and further apart further out
function voicePlaces(waves) {
    const centre = tableCentre();
    const chorus = chorusAmount();

    return gearVoices.map((k) => {
        const away = ((VOICE_SPREAD[k] + 1) / 2) - centre;  // Spread evenly
        const room = (away < 0) ? centre : 1 - centre;      // To that end
        const out = ((k === 0) || (room === 0)) ? 0
            : Math.sign(away) * room * ((Math.abs(away) / room) ** CHORUS_BUNCHING);

        return (centre + (chorus * out)) * (waves - 1);
    });
}

// The two waves of a table a place is between: the lower one, and how far
// the place is on to the next
function between(place, waves) {
    const low = Math.min(Math.floor(place), waves - 2);

    return { low, part: place - low };
}

// The wave at a place in a table, for an oscillator: the two waves it is
// between, mixed harmonic by harmonic
function waveAt(name, place) {
    const waves = tableWaves(name);
    const { low, part } = between(place, waves.length);
    const mix = (from, to) => from.map((value, n) => ((1 - part) * value) + (part * to[n]));

    return gear.context.createPeriodicWave(mix(waves[low].real, waves[low + 1].real),
        mix(waves[low].imag, waves[low + 1].imag), { disableNormalization: true });
}

// The wave chosen and the chorus, put on the gear's voices: each voice's
// wave, how far off pitch it is and how much of it there is. On a wavetable
// each has the wave at its own place; on one of the oscillator's own waves
// all have that wave, so the chorus is only the voices being off pitch. The
// gains add up to 1, so the VCO's output keeps its full scale
function applyVoices(wave) {
    const chorus = chorusAmount();
    const places = (WAVETABLES[wave] === undefined)
        ? null : voicePlaces(WAVETABLES[wave].length);
    const amounts = voicePresences().map(
        (presence, k) => ((k === 0) ? 0 : (chorus * presence) / (CHORUS_VOICES + 1)));

    amounts[0] = 1 - amounts.reduce((sum, amount) => sum + amount, 0);
    gear.oscillators.forEach((oscillator, k) => {
        // A wave is made only for a voice that is heard and has moved
        const held = (places === null) ? wave : `${wave} ${places[k].toFixed(3)}`;

        if ((amounts[k] > 0) && (held !== gear.held[k])) {
            gear.held[k] = held;
            if (places === null) {
                oscillator.type = wave;
            } else {
                oscillator.setPeriodicWave(waveAt(wave, places[k]));
            }
        }
        oscillator.detune.value = chorus * CHORUS_DETUNE_CENTS * VOICE_SPREAD[k];
        gear.gains[k].gain.value = amounts[k];
    });
}

// The wave chosen, put on the gear: the oscillators, or the noise with
// them switched out and left as they are
function applyWave() {
    const wave = el('vco-wave').value;
    const isNoise = wave === VCO_NOISE;

    if (!isNoise) {
        applyVoices(wave);
    }
    gear.oscillatorOn.gain.value = isNoise ? 0 : 1;
    gear.noiseOn.gain.value = isNoise ? 1 : 0;
}

// What the position pot is doing, whether the gear is on or not
function positionReading() {
    const waves = WAVETABLES[el('vco-wave').value];

    if (waves === undefined) {
        return 'no use on this wave';
    }
    const { low, part } = between(voicePlaces(waves.length)[0], waves.length);

    return `${waves[low].name} to ${waves[low + 1].name}, ${Math.round(part * 100)}%`;
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

// A pot that is off at 0, worded: its part of the way up as a percentage
const offOrPercent = (part) => ((part === 0) ? 'off' : `${Math.round(part * 100)}%`);

// The gear's knobs, worded, and put on the gear if it is on
function applyGear() {
    const tune = Number(el('vco-tune').value);
    const wave = el('vco-wave').value;
    const isTable = WAVETABLES[wave] !== undefined;
    // The pots that do nothing on the wave chosen, which are dimmed
    const unused = { 'vco-tune': wave === VCO_NOISE, chorus: wave === VCO_NOISE,
        'vco-position': !isTable, 'env-position': !isTable };

    for (const [id, isUnused] of Object.entries(unused)) {
        el(id).closest('.pots > div').classList.toggle('unused', isUnused);
    }
    el('vco-tune-reading').textContent = `${(tune > 0) ? '+' : ''}${tune} semitones`;
    el('vco-position-reading').textContent = positionReading();
    el('chorus-reading').textContent = offOrPercent(chorusAmount());
    el('env-attack-reading').textContent = `${el('env-attack').value} ms`;
    el('env-release-reading').textContent = `${el('env-release').value} ms`;
    el('env-position-reading').textContent = offOrPercent(sweepAmount());
    el('volume-reading').textContent = `${el('volume').value}%`;
    el('gear-power').textContent = `Gear: ${(gear === null) ? 'off' : 'on'}`;
    el('gear-power').setAttribute('aria-pressed', String(gear !== null));
    el('gear-hint').hidden = gear !== null;
    if (gear !== null) {
        applyWave();
        gear.volume.gain.value = Number(el('volume').value) / VOLUME_STEPS;
        gearPitch(lastValue(trace.cv));
    }
    drawWavetable();
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

// The band the VCO's frequency is drawn in, as { low, high } in octaves
// over VCO_BASE_HZ: a whole number of octaves high, so a semitone is the
// same rise wherever it is. Null on white noise, which has no frequency
function pitchRange() {
    // Trimmed here, not left to drawSignal(), which gets a copy; and before
    // the noise is let off, or the points would pile up while it plays
    trim(gear.pitchPoints);
    if (el('vco-wave').value === VCO_NOISE) {
        return null;
    }
    const octaves = gear.pitchPoints.map((point) => point[1]);
    const low = Math.floor(Math.min(...octaves));

    return { low, high: Math.max(low + 1, Math.ceil(Math.max(...octaves))) };
}

const octaveHertz = (octaves) => (VCO_BASE_HZ * (2 ** octaves)).toFixed(0);

// The scope's axes. A wave's band: its zero and its two peaks, and a line
// every SCOPE_TICK_MS. The frequency's: a line at each octave, named in
// hertz. The envelope's: its 0 and 1. Those two have the trace's seconds
function drawScopeAxes(context, colour, top, bandHeight, width, spanMs, range) {
    const ticks = [];
    const octaves = [];

    for (let ms = SCOPE_TICK_MS; ms < spanMs; ms += SCOPE_TICK_MS) {
        ticks.push([ms / spanMs, `${ms} ms`]);
    }
    for (const band of [0, 3]) {
        drawAxes(context, colour, top(band), bandHeight, width, SCOPE_WAVE_LEVELS, ticks);
    }
    if (range !== null) {
        for (let octave = range.low; octave <= range.high; octave++) {
            octaves.push([(octave - range.low) / (range.high - range.low),
                `${octaveHertz(octave)} Hz`]);
        }
        drawAxes(context, colour, top(1), bandHeight, width, octaves, secondsBack());
    }
    drawAxes(context, colour, top(2), bandHeight, width, SCOPE_ENVELOPE_LEVELS,
        secondsBack());
}

// Four bands: the VCO's output and the audio output as waves a few
// milliseconds long, and between them the VCO's frequency and the envelope
// over the trace's window
function drawScope(time) {
    const { context, width, height, colour } = openCanvas(el('scope'));
    const row = height / SCOPE_ROWS;
    const bandHeight = row - SCOPE_LABEL_PX - SCOPE_GAP_PX;
    const top = (n) => (row * n) + SCOPE_LABEL_PX;
    const label = (text, n) => context.fillText(text, 4, (row * n) + 12);

    context.fillStyle = colour('--dim');
    if (gear === null) {
        label('The gear is off: turn it on to hear the board and see it here', 0);
    } else {
        const spanMs = (SCOPE_SAMPLES * MS_PER_S) / gear.context.sampleRate;
        const seconds = TRACE_WINDOW_MS / MS_PER_S;

        readProbes();
        followGear(time);

        const start = triggerAt(gear.vcoProbe.samples, SCOPE_PROBE_SAMPLES - SCOPE_SAMPLES);
        const range = pitchRange();

        label(`VCO out, ${spanMs.toFixed(0)} ms`, 0);
        label((range === null) ? 'VCO frequency: white noise has none'
            : `VCO frequency, ${octaveHertz(range.low)} to ${octaveHertz(range.high)} Hz, `
                + `${seconds} s`, 1);
        label(`Envelope into the VCA's CV, ${seconds} s`, 2);
        label(`Audio out, after the VCA, ${spanMs.toFixed(0)} ms`, 3);
        if (el('scope-axes').checked) {
            drawScopeAxes(context, colour, top, bandHeight, width, spanMs, range);
        }
        drawWave(context, gear.vcoProbe.samples, start, colour('--cv'),
            top(0), bandHeight, width);
        if (range !== null) {
            drawSignal(context,
                gear.pitchPoints.map(([ms, value]) => [ms, value - range.low]),
                colour('--cv'), top(1), bandHeight, range.high - range.low, width);
        }
        drawSignal(context, gear.envelopePoints, colour('--gate'),
            top(2), bandHeight, 1, width);
        drawWave(context, gear.outProbe.samples, start, colour('--on'),
            top(3), bandHeight, width);
    }
    requestAnimationFrame(drawScope);
}
