'use strict';
/**
 * Uneven pulse trains for a clock source: swung pairs of edges, and two
 * random clocks made the way a noise source into a gate makes one. Each is a
 * function for clock_source.js's start(), saying how long the pulse now
 * rising is high and how long until the next one rises. Time is in whatever
 * unit the period is given in. Knows nothing about what it is plugged into.
 *
 * The random ones draw from a seeded generator, so the same seed gives the
 * same train on every run.
 */

// A swung clock's edges come in pairs, long gap then short, a pair lasting
// two periods. The figure is the long gap's share of the pair
const SWING_SHARES = {
    swing54: 0.54,
    swing58: 0.58,
    swing62: 0.62,
    swing67: 2 / 3,     // Triplet swing
    swing75: 0.75,      // Dotted
};
const RANDOM_PATTERNS = ['white', 'pink'];
const CLOCK_PATTERNS = ['steady', ...Object.keys(SWING_SHARES), ...RANDOM_PATTERNS];
const DEFAULT_SEED = 0x2545f491;
const UINT32_RANGE = 0x100000000;
const MAX_GAP_SLOTS = 16;   // The longest a random clock goes without an edge
const PINK_ROWS = 4;

/** A xorshift generator: gives a number from 0 up to but not including 1. */
function makeRandom(seed) {
    let state = (seed >>> 0) || DEFAULT_SEED; // It cannot start from 0

    return () => {
        state ^= state << 13;
        state ^= state >>> 17;
        state ^= state << 5;
        state >>>= 0;
        return state / UINT32_RANGE;
    };
}

/**
 * White noise into a gate: every slot has an even chance of an edge, whatever
 * the slots before it did. Gives a function saying whether the next slot
 * has one.
 */
function makeWhiteGate(random) {
    return () => random() < 0.5;
}

/**
 * Pink noise into a gate. The noise is the sum of rows of random numbers,
 * each row redrawn half as often as the one before it (Voss and McCartney's
 * way), and a slot has an edge while the sum is over its middle. The slow
 * rows hold it one side or the other for a while, so the edges come in
 * bursts with lulls between.
 */
function makePinkGate(random) {
    const rows = Array.from({ length: PINK_ROWS }, random);
    let slot = 0;

    return () => {
        let row = 0;

        slot++;
        // The row to redraw is the number of times 2 divides the slot
        while ((row < (PINK_ROWS - 1)) && (((slot >> row) & 1) === 0)) {
            row++;
        }
        rows[row] = random();
        return rows.reduce((sum, value) => sum + value, 0) > (PINK_ROWS / 2);
    };
}

/**
 * Makes a pulse train of a named pattern.
 * @param name    One of CLOCK_PATTERNS.
 * @param period  The time between edges of a steady clock. A swung clock
 *                averages it; a random one has slots this long, with an
 *                edge on half of them, more or less.
 * @param seed    Optional. Starts the random patterns' generator.
 * @return A function giving { gap, high } for each pulse in turn: the time
 *         from its rising edge to the next one's, and how long it is high.
 */
function makeClockPattern(name, period, seed = DEFAULT_SEED) {
    if (name === 'steady') {
        return () => ({ gap: period, high: period / 2 });
    }
    if (name in SWING_SHARES) {
        const longGap = 2 * period * SWING_SHARES[name];
        let isShortNext = false;

        return () => {
            const gap = isShortNext ? ((2 * period) - longGap) : longGap;

            isShortNext = !isShortNext;
            return { gap, high: gap / 2 };
        };
    }
    if (RANDOM_PATTERNS.includes(name)) {
        const random = makeRandom(seed);
        const hasEdge = (name === 'white') ? makeWhiteGate(random) : makePinkGate(random);

        return () => {
            let slots = 1;

            while ((slots < MAX_GAP_SLOTS) && !hasEdge()) {
                slots++;
            }
            return { gap: slots * period, high: period / 2 };
        };
    }
    throw new Error(`No clock pattern called ${name}`);
}

module.exports = { makeClockPattern, CLOCK_PATTERNS };
