'use strict';
// The page's tables, the state its scripts share, and post()
const STEP_COUNT = 16;
const NOTE_BITS = 4;
const STEPS_PER_ROW = 8;
const STEPS_PER_GROUP = 4;
const MODE_BITS = 8;
// The low three mode switches choose the engine, as a number. Every number
// with no engine of its own plays the plain one. On the panel they are one
// rotary switch of eight positions that puts out its position as that number
const ENGINE_BITS = 3;
const ENGINE_MASK = (1 << ENGINE_BITS) - 1;
// The engine's rotary switch and the selector's are the same part, drawn
// alike: eight positions, in the units of its own picture: where its
// middle is, how big it is, how far out the dots and the names stand, and
// the angle of position 0 and of each position on from it, clockwise from up
const KNOB_POSITIONS = 8;
const KNOB_WIDTH = 230;
const KNOB_HEIGHT = 112;
const KNOB_X = 115;
const KNOB_Y = 70;
const KNOB_RADIUS = 26;
const KNOB_DOT_RADIUS = 35;
const KNOB_LABEL_RADIUS = 46;
const KNOB_FIRST_DEG = -126;
const KNOB_STEP_DEG = 36;
// A pot as drawn, in the units of its own picture: its size, its middle,
// how big it is, how far out the dots at the ends of its travel stand, and
// how far it turns from one end to the other. Then how far a drag has to
// go, in pixels, to take it the whole way
const POT_SIZE = 44;
const POT_MIDDLE = 22;
const POT_RADIUS = 15;
const POT_DOT_RADIUS = 20;
const POT_SWEEP_DEG = 270;
const POT_DRAG_PX = 200;
const ENGINE_PLAIN = 0;
const ENGINE_STAGE = 1;
const ENGINE_KLEE = 2;
const ENGINE_CHANCE = 3;
const ENGINE_CARTESIAN = 4;
const ENGINE_EUCLID = 5;
const ENGINE_NAMES = ['plain', 'stage', 'Klee', 'chance', 'Cartesian', 'Euclidean'];
// The class the panel takes for each engine that has terms of its own to
// read the steps in
const ENGINE_CLASSES = ['', 'stage', 'klee', '', 'cartesian', 'euclid'];
// What is said over each of the panel's two rows, for an engine to which
// the rows mean something
const ROW_CAPTIONS = {
    [ENGINE_STAGE]: ['Stages 0 to 7: their notes',
        'Their controls: length and gate mode'],
    [ENGINE_CARTESIAN]: ['Grid rows y 0 and y 1', 'Grid rows y 2 and y 3'],
};
// The clock source's patterns that are random: the rate is then a slot, with
// an edge on half the slots
const RANDOM_CLOCK_PATTERNS = ['white', 'pink'];
const RANDOM_SLOTS_PER_EDGE = 2;
// Mode switches 3 to 7 are the chosen engine's own. Klee: 3 load, 4 invert,
// 5 merge, 6 and 7 the chance as a number; a step's highest switch is its
// pattern switch and its low three its value
const KLEE_LOAD_BIT = 3;
const KLEE_INVERT_BIT = 4;
const KLEE_MERGE_BIT = 5;
const KLEE_CHANCE_SHIFT = 6;
const KLEE_CHANCE_OF = 8;
const KLEE_PATTERN_SWITCH = 0x08;
const KLEE_VALUE_MASK = 0x07;
// Chance: 3 and 4 the skips in four as a number, 5 shuffle
const CHANCE_SKIP_SHIFT = 3;
const CHANCE_SKIP_OF = 4;
const CHANCE_SHUFFLE_BIT = 5;
// Cartesian: 3 and 4 set how often x moves, 5 and 6 how often y does
const GRID_SIDE = 4;
const GRID_X_SHIFT = 3;
const GRID_Y_SHIFT = 5;
// Euclidean: 3 to 6 as a number are the beats in a bar of sixteen, less one
const EUCLID_BEATS_SHIFT = 3;
const EUCLID_BEATS_MASK = 0x0f;
const EUCLID_BAR_STEPS = 16;
const TWO_SWITCH_MASK = 0x03;
const NO_USE = 'no use';
// The stage engine: stage n's note is step n and its control step n + 8. A
// control's low two switches are the length less one, its high two the mode
const STAGE_COUNT = 8;
const STAGE_LENGTH_MASK = 0x03;
const STAGE_MODE_SHIFT = 2;
const STAGE_MODES = ['single', 'repeat', 'hold', 'rest'];
const CAL_ENTRY_NOTES = [10, 5, 10, 5]; // On the last four steps
const STEPS_PER_BEAT = 4;
const MS_PER_MINUTE = 60000;
const ADC_STEPS = 1024;
const ADC_REFERENCE_VOLTS = 5;
const TRACE_WINDOW_MS = 4000;
const CV_SCALE_MIN_MV = 1250;   // The trace's full scale, unless the CV is higher
const CV_SCALE_STEP_MV = 1000;
const LOG_LINES_MAX = 300;
const MIDI_LINES_MAX = 200;
const MIDI_CLOCKS_PER_QUARTER = 24;
// How a line of the MIDI monitor names each message, padded to one width
const MIDI_NAMES = { start: 'Start', stop: 'Stop', noteOn: 'Note On ', noteOff: 'Note Off' };
const MS_PER_S = 1000;
// The patched gear. The VCO: what 0 V plays with the tune knob at its
// middle, an octave up per volt, and the wave that is not an oscillator's
const VCO_BASE_HZ = 130.81;
const VCO_NOISE = 'noise';
const SEMITONES_PER_OCTAVE = 12;
// The VCO's wavetables. A wave is its harmonics: `part(n)` gives harmonic
// n's cosine and sine amounts, up to WAVETABLE_HARMONICS. The position pot
// goes through a table from its first wave to its last
const WAVETABLE_HARMONICS = 32;
const POSITION_STEPS = 1000;    // The position pot's top
const HARMONIC_WAVES = 16;
const sawPart = (n) => [0, (((n % 2) === 1) ? 1 : -1) / n];
// A pulse high for `duty` of its cycle from the start of it, so its rising
// edge is where the other waves rise through zero and a mix keeps its level
const pulsePart = (duty) => (n) => [Math.sin(2 * Math.PI * n * duty) / n,
    (1 - Math.cos(2 * Math.PI * n * duty)) / n];
// The harmonics of a wave given as its height through a cycle, the phase
// going from 0 to 1: found by adding it up at ANALYSIS_POINTS places
const ANALYSIS_POINTS = 256;
const shapePart = (height) => (n) => {
    let cosine = 0;
    let sine = 0;

    for (let j = 0; j < ANALYSIS_POINTS; j++) {
        const angle = (2 * Math.PI * n * j) / ANALYSIS_POINTS;

        cosine += height(j / ANALYSIS_POINTS) * Math.cos(angle);
        sine += height(j / ANALYSIS_POINTS) * Math.sin(angle);
    }
    return [(2 * cosine) / ANALYSIS_POINTS, (2 * sine) / ANALYSIS_POINTS];
};
// The tables made as a series are each this many waves, wave i from make(i)
const SERIES_WAVES = 8;
const seriesOf = (make) => Array.from({ length: SERIES_WAVES }, (unused, i) => make(i));
const PWM_DUTIES = [50, 40, 30, 20, 15, 10, 7, 4];     // Percent
const FORMANT_GAIN = 6;         // How far the peak stands over the saw
const FORMANT_WIDTH = 1.5;      // And how many harmonics wide it is
const SYNC_STEP = 0.5;          // How much further each synced wave is tuned up
const WAVETABLES = {
    classic: [
        { name: 'sine', part: (n) => [0, (n === 1) ? 1 : 0] },
        { name: 'triangle', part: (n) => [0, ((n % 2) === 0) ? 0
            : (((n % 4) === 1) ? 1 : -1) / (n * n)] },
        { name: 'saw', part: sawPart },
        { name: 'square', part: pulsePart(0.5) },
        { name: '25% pulse', part: pulsePart(0.25) },
        { name: '10% pulse', part: pulsePart(0.1) },
    ],
    // A saw let through a harmonic at a time, like a filter opening
    harmonics: Array.from({ length: HARMONIC_WAVES }, (unused, i) => ({
        name: (i === 0) ? 'harmonic 1' : `harmonics 1 to ${i + 1}`,
        part: (n) => ((n <= (i + 1)) ? sawPart(n) : [0, 0]),
    })),
    // A pulse narrowing from a square: pulse width modulation by hand
    pwm: PWM_DUTIES.map((percent) => ({
        name: `${percent}% pulse`, part: pulsePart(percent / 100),
    })),
    // A square built up an odd harmonic at a time: hollow, then buzzing
    odd: seriesOf((i) => ({
        name: (i === 0) ? 'harmonic 1' : `odd harmonics to ${(2 * i) + 1}`,
        part: (n) => ((((n % 2) === 1) && (n <= ((2 * i) + 1))) ? [0, 1 / n] : [0, 0]),
    })),
    // A saw with a peak in its harmonics that goes up them, as a vowel's or
    // a resonant filter's does
    formant: seriesOf((i) => ({
        name: `peak at harmonic ${(2 * i) + 2}`,
        part: (n) => sawPart(n).map((amount) => amount * (1 + (FORMANT_GAIN
            * Math.exp(-((((n - ((2 * i) + 2)) / FORMANT_WIDTH)) ** 2))))),
    })),
    // A sine driven harder and harder into a wavefolder
    fold: seriesOf((i) => ({
        name: `folded ×${i + 1}`,
        part: shapePart((phase) => Math.sin(((i + 1) * Math.PI * Math.sin(
            2 * Math.PI * phase)) / 2)),
    })),
    // A saw synced to the VCO and tuned up from it: hard sync
    sync: seriesOf((i) => ({
        name: `synced ×${(1 + (i * SYNC_STEP)).toFixed(1)}`,
        part: shapePart((phase) => (2 * ((phase * (1 + (i * SYNC_STEP))) % 1)) - 1),
    })),
};
// The wavetable's picture: the points a wave's cycle is drawn through, and
// as parts of the canvas where the front wave starts, a wave's width and
// half height, and how far right and up the back wave is from the front.
// Then the room under the front wave for its name, and the chosen wave's line
const WAVE_PICTURE_POINTS = 96;
const PICTURE_LEFT = 0.04;
const PICTURE_WAVE_WIDTH = 0.6;
const PICTURE_WAVE_HEIGHT = 0.14;
const PICTURE_DEPTH_X = 0.32;
const PICTURE_DEPTH_Y = 0.5;
const PICTURE_FOOT_PX = 18;
const PICTURE_CHOSEN_WIDTH = 2.5;
// How many lines the picture draws a table with, about: its waves and as
// many between each two as bring it near this. Then the line of each of
// the chorus's voices, and how solid it is
const PICTURE_LINES_ABOUT = 72;
const PICTURE_TABLE_WIDTH = 1;  // A line of the table itself
const PICTURE_VOICE_WIDTH = 1.5;
const PICTURE_VOICE_ALPHA = 0.7;
// The chorus: the voices it adds to the VCO's first, how far off pitch the
// outermost two are at the top of the chorus pot, and the pot's top
const CHORUS_VOICES = 36;
const CHORUS_DETUNE_CENTS = 15;
const CHORUS_STEPS = 100;
// The chorus's voices stand evenly from one end of its spread to the
// other. This is which of those stands each takes, in the order the voices
// come in as the chorus pot comes up: the two ends first, then each one in
// the middle of the widest gap left
const CHORUS_STANDS = [0, CHORUS_VOICES - 1];
while (CHORUS_STANDS.length < CHORUS_VOICES) {
    const taken = [...CHORUS_STANDS].sort((a, b) => a - b);
    let widest = 1;

    for (let i = 2; i < taken.length; i++) {
        if ((taken[i] - taken[i - 1]) > (taken[widest] - taken[widest - 1])) {
            widest = i;
        }
    }
    CHORUS_STANDS.push(Math.floor((taken[widest - 1] + taken[widest]) / 2));
}
// How much the voices' places are drawn in towards the VCO's first voice:
// 1 would leave them evenly spread, more stands them closer together near it
const CHORUS_BUNCHING = 1.6;
// The first two voices are there from the start, and the number goes up
// by the same ratio for each step up the pot, to all of them at its top.
// Any voice takes this much of the pot to come in fully
const CHORUS_FIRST_VOICES = 2;
const CHORUS_VOICE_FADE = 0.06;
// The envelope's position pot's top, and how far the envelope has to move
// before the wavetable is moved with it
const SWEEP_STEPS = 100;
const ENVELOPE_STEP = 0.002;
// The picture's scan along the chosen wave: how often it goes along it at
// VCO_BASE_HZ, twice as often an octave up, and the most often it goes.
// Then how much of the wave it is long, its line and its glow
const PICTURE_SCAN_HZ = 1;
const PICTURE_SCAN_MAX_HZ = 8;
const PICTURE_SCAN_TAIL = 0.12;
const PICTURE_SCAN_WIDTH = 5;
const PICTURE_VOICE_SCAN_WIDTH = 3;     // On a voice of the chorus
const PICTURE_SCAN_GLOW_PX = 10;
// Every voice by number: 0 the VCO's first, the rest the chorus's
const gearVoices = Array.from({ length: CHORUS_VOICES + 1 }, (unused, k) => k);
const NOISE_SECONDS = 2;        // Before the noise comes round again
// An attack or release is the time to get about 95% of the way, which is
// this many of the time constants the envelope is given
const ENVELOPE_SETTLES = 3;
const VOLUME_STEPS = 100;       // The volume pot's top
// The scope: how much of a signal a probe holds, how much of that a wave
// is drawn from, and its four bands, each under a label
const SCOPE_PROBE_SAMPLES = 2048;
const SCOPE_SAMPLES = 1024;
const ENVELOPE_PROBE_SAMPLES = 32;
const SCOPE_ROWS = 4;
const SCOPE_LABEL_PX = 16;
const SCOPE_GAP_PX = 4;
// What the tempo pot's reading means, as the firmware takes it: a tempo on
// the internal clock, and one of seven ratios to the external clock
const BPM_MIN = 30;
const LFO_CENTRE = 512;   // Where the modulation input moves nothing
const LFO_DEAD_ZONE = 16; // How far from it still moves nothing
// What each selector position's amount is for; 5 to 7 do nothing yet
const SELECTOR_NAMES = [
    '0 Tempo', '1 Note', '2 Gate length', '3 Step chance',
    '4 Step position', '5 Amplitude (not built)',
    '6 Attack/release (not built)', '7 Vibrato (not built)',
];
// What the selector's rotary switch has at each position that does
// something; the rest get their number, as the engine's do
const SELECTOR_KNOB_NAMES = ['tempo', 'note', 'gate length', 'step chance'];
const READING_PER_BPM = 4;
const READING_PER_RATIO = 147;
const RATIOS = [
    ['÷4', 'a step every 4 edges'], ['÷3', 'a step every 3 edges'],
    ['÷2', 'a step every 2 edges'], ['×1', 'a step to an edge'],
    ['×2', '2 steps to an edge'], ['×3', '3 steps to an edge'],
    ['×4', '4 steps to an edge'],
];
const TRANSPORT_STOPPED = 0;    // As the firmware numbers them in its log
const TRANSPORT_EXTERNAL = 2;
const TRANSPORT_NAMES = ['Stopped', 'Running on the internal clock',
    'Running on the external clock'];
// A crash log record's place, as the firmware numbers what it was doing,
// and its cause, which is the chip's reset flags
const PLACE_NAMES = { 1: 'past start-up, no tick run yet', 2: 'applying outputs',
    3: 'gathering inputs', 4: 'in the core', 5: 'between ticks' };
const PLACE_UNMARKED = 'none of the marks';
const CAUSE_WATCHDOG = 8;

const el = (id) => document.getElementById(id);
// cv, gate and clock are each a list of [ms, value]; clockIn and resetIn
// list the ms of each edge that arrived
const trace = { cv: [], gate: [], clock: [], clockIn: [], resetIn: [] };
let state = null;       // The last `state` event
let nowMs = 0;          // Chip time
let lastClockRiseMs = null;
let gear = null;        // The patched gear, while it is on

function post(command) {
    return fetch('/control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(command),
    });
}
