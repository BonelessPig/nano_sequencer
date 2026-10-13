'use strict';
// The status light: lit while the selected profile holds something, winking
// as a hold of the profile button passes each of its times, and flickering
// to say what was done, on the emulated board, for all three firmware
// images. A flicker is the light turned the other way for 80 ms, and what
// could not be done is one of 500 ms.
const assert = require('node:assert/strict');
const { test } = require('node:test');
const { createBoard, CONFIGS } = require('../boards/nano_sequencer');
const { eepromWithProfiles, PROFILES_END } = require('../boards/profile_record');
const { READING_150_BPM, nowMs } = require('./fixture');

const PANEL = new Array(16).fill(5);
const AMOUNTS = [1023, 0, 0, 0, 0, 0, 0];   // Any profile that holds something
const START_MS = 30;             // Every profile looked at, and a scan
const SCAN_MS = 20;              // More than two input scans
const CLICK_MS = 150;
const SAVE_MS = 1300;            // Holds that end clear of the wink before
const CLEAR_MS = 5300;
const SEEN_MS = 30;              // A scan, the debounce and a tick: the hold
                                 // is counted from this long after the press
const SEEN_SLACK_MS = 12;
const RUN_ON_MS = 800;           // Longer than any run of flickers
const FLICKER_MS = 80;
const LONG_MS = 500;
const WIDTH_SLACK_MS = 1.5;      // A tick, and where in it the pin is written

function startedBoard(config, { eeprom, selector = 0 } = {}) {
    const board = createBoard(config, { eeprom });

    board.setNotes(PANEL);
    board.setTempoReading(READING_150_BPM);
    board.setSelector(selector);
    board.runForMs(START_MS);
    return board;
}

// Every change of the light since a time: when, and whether it came on
function changesSince(board, fromMs) {
    return board.statusLight.edges
        .map((edge) => ({ ms: board.machine.cyclesToMs(edge.cycle), lit: edge.high }))
        .filter((edge) => edge.ms > fromMs);
}

// The lengths of the stretches between one change and the next
function stretches(changes) {
    return changes.slice(1).map((edge, i) => edge.ms - changes[i].ms);
}

function assertNear(actual, expected, slack, what) {
    assert.ok(Math.abs(actual - expected) <= slack, `${what}: ${actual}, not near ${expected}`);
}

function assertAllNear(values, expected, slack, what) {
    for (const value of values) {
        assertNear(value, expected, slack, what);
    }
}

for (const config of CONFIGS) {
    test(`${config}: the status light is dark with nothing stored`, () => {
        const board = startedBoard(config, { selector: 3 });

        board.runForMs(300);
        assert.equal(board.statusLight.edges.length, 0);
        assert.equal(board.statusLit(), false);
    });

    test(`${config}: it is lit from power-on while the selected profile holds something`, () => {
        const eeprom = eepromWithProfiles({ 3: AMOUNTS });
        const board = startedBoard(config, { eeprom, selector: 3 });

        // Once every profile has been looked at, within 20 ms, and steadily
        assert.equal(board.statusLit(), true);
        assert.equal(board.statusLight.edges.length, 1);
        assert.ok(nowMs(board) >= changesSince(board, 0)[0].ms);
        assert.ok(changesSince(board, 0)[0].ms < 20);

        board.setSelector(4);
        board.runForMs(SCAN_MS);
        assert.equal(board.statusLit(), false);
        board.setSelector(3);
        board.runForMs(SCAN_MS);
        assert.equal(board.statusLit(), true);
        board.runForMs(300);
        assert.equal(board.statusLight.edges.length, 3);
    });

    test(`${config}: a save winks at one second, flickers twice and leaves it lit`, () => {
        const board = startedBoard(config, { selector: 2 });
        const pressedMs = nowMs(board);

        board.setProfileButton(true);
        board.runForMs(SAVE_MS);

        // The wink, while the button is still down: lit for 80 ms
        const wink = changesSince(board, pressedMs);
        assert.deepEqual(wink.map((edge) => edge.lit), [true, false]);
        assertNear(wink[0].ms - pressedMs, 1000 + SEEN_MS, SEEN_SLACK_MS, 'the wink');
        assertAllNear(stretches(wink), FLICKER_MS, WIDTH_SLACK_MS, 'the wink');

        // The profile holds something from here, so the first flicker is
        // dark, as the light was, and only the second is seen to begin
        const releasedMs = nowMs(board);
        board.setProfileButton(false);
        board.runForMs(RUN_ON_MS);
        const after = changesSince(board, releasedMs);
        assert.deepEqual(after.map((edge) => edge.lit), [true, false, true]);
        assertAllNear(stretches(after), FLICKER_MS, WIDTH_SLACK_MS, 'a flicker');
        assert.equal(board.statusLit(), true);

        // And it is lit at the next power-on
        const eeprom = board.eepromBytes(PROFILES_END);
        const again = startedBoard(config, { eeprom, selector: 2 });
        assert.equal(again.statusLit(), true);
    });

    test(`${config}: a recall flickers once`, () => {
        const eeprom = eepromWithProfiles({ 5: AMOUNTS });
        const board = startedBoard(config, { eeprom, selector: 5 });
        const pressedMs = nowMs(board);

        board.setProfileButton(true);
        board.runForMs(CLICK_MS);
        board.setProfileButton(false);
        board.runForMs(RUN_ON_MS);

        const changes = changesSince(board, pressedMs);
        assert.deepEqual(changes.map((edge) => edge.lit), [false, true]);
        assertAllNear(stretches(changes), FLICKER_MS, WIDTH_SLACK_MS, 'the flicker');
    });

    test(`${config}: a recall of a profile that holds nothing is one long blink`, () => {
        const board = startedBoard(config, { selector: 5 });
        const pressedMs = nowMs(board);

        board.setProfileButton(true);
        board.runForMs(CLICK_MS);
        board.setProfileButton(false);
        board.runForMs(RUN_ON_MS);

        const changes = changesSince(board, pressedMs);
        assert.deepEqual(changes.map((edge) => edge.lit), [true, false]);
        assertAllNear(stretches(changes), LONG_MS, WIDTH_SLACK_MS, 'the long blink');
    });

    test(`${config}: a clear winks twice, flickers three times and leaves it dark`, () => {
        const eeprom = eepromWithProfiles({ 6: AMOUNTS });
        const board = startedBoard(config, { eeprom, selector: 6 });
        const pressedMs = nowMs(board);

        board.setProfileButton(true);
        board.runForMs(CLEAR_MS);

        // Dark for 80 ms as the hold passes one second, and five
        const winks = changesSince(board, pressedMs);
        assert.deepEqual(winks.map((edge) => edge.lit), [false, true, false, true]);
        assertNear(winks[0].ms - pressedMs, 1000 + SEEN_MS, SEEN_SLACK_MS, 'the first wink');
        assertNear(winks[2].ms - winks[0].ms, 4000, WIDTH_SLACK_MS, 'the second wink');
        assertNear(winks[1].ms - winks[0].ms, FLICKER_MS, WIDTH_SLACK_MS, 'the first wink');
        assertNear(winks[3].ms - winks[2].ms, FLICKER_MS, WIDTH_SLACK_MS, 'the second wink');

        // The profile holds nothing from here, so the first flicker is lit,
        // as the light was, and the third ends dark
        const releasedMs = nowMs(board);
        board.setProfileButton(false);
        board.runForMs(RUN_ON_MS);
        const after = changesSince(board, releasedMs);
        assert.deepEqual(after.map((edge) => edge.lit), [false, true, false, true, false]);
        assertAllNear(stretches(after), FLICKER_MS, WIDTH_SLACK_MS, 'a flicker');
        assert.equal(board.statusLit(), false);
    });
}
