---
paths:
  - "tools/sim/scenarios/*.js"
  - "tools/sim/boards/nano_sequencer.js"
---

# tools/sim: writing a scenario

Loaded with the files below, on top of `tools/sim/CLAUDE.md`, whose rules apply to all of `tools/sim/`.

- `boards/nano_sequencer.js`: which part is on which pin, plus the panel, the mode switches (`setModeSwitches()`), the selector (`setSelector()`), the profile button (`setProfileButton()`), the level light (`takeLevel()`), the modulation input (`setLfoReading()`), the LFO rate pot (`setRateReading()`), the step lights (`ledLatches`, `ledsAt()`), pot, the DAC's frames (`cvWrites` and `cvWriteBefore()` for the pitch on channel A, `envWrites` and `envMillivoltsAt()` for the envelope on channel B), the gate and clock recorders, the two input jacks and the run button, what the USART sent (the debug image's log sorted into lines in `serial`, the midi image's bytes and the messages they make in `midi`), the EEPROM's contents and the reset flags at power-on (`createBoard(config, { eeprom, resetFlags })`, the config one of `CONFIGS`: debug, release or midi), and for the watchdog `stopTick()`, the cycle of each reset (`resets`) and the crash log read back from the EEPROM (`crashLog()`).
- Every run is bounded: use `runUntil` with a limit so a scenario fails instead of hanging.
- Time a step from the first byte of its log line (`startCycle`), not the last: the line is sent a byte per loop pass, so its end moves with its length.
- The release and midi images have no log.
  Time their steps from the clock output, which pulses on every step, and its notes from the gate.
  The same measurements work on the debug image, where they are far steadier than the log (0.003 ms against 0.25 ms).
- A scenario goes in the file for its topic, inside that file's loop over the three images if it should hold for every one: `transport.test.js`, `calibration.test.js`, `engines.test.js` (the mode switches or an engine), `watchdog.test.js`, `modulation.test.js`, `profiles.test.js`, and `midi.test.js` or `midi_clock.test.js` for the midi image alone.
- The step the pattern has got to, on any image, is the number of clock output rises before the gate rise in question: the gate is written ahead of the clock output in a tick.
- The board sorts the log: `serial.lines` holds the step lines, which scenarios count and time, and `serial.notices` every other line (start-up, calibration, errors).
  A new kind of line goes to the notices unless it begins `Step `.
- There is one wire from the USART and the board knows from its config what is on it.
  A new image is a line in `WIRES` in the board.
- A power cycle is a second board created with the first one's `eepromBytes()`.
  A scenario with no log (the release and midi images, or calibration mode in any) runs for a time with `runForMs()`, not until a line.
- The chain is ten chips, so a panel read is 80 clocks and ten transfers.
  The profile button is the tenth chip's fourth input; hold it with `runForMs()` and allow 60 ms after letting go for the debounce and a scan, and 80 ms more for a save to reach the EEPROM.
  A profile is seen through what the LFO does with its amounts, from the second step on: the first step begins before any LFO has been read.
  The selector is at 0 on a new board, so its pot sets the tempo, and a reading set after that changes the tempo as it always did; turned to another position, the pot has to meet what that position holds before it changes anything.
  The mode switches are off on a new board, which is the plain engine, so a scenario that does not set them plays sixteen steps.
- `node --test` on one scenario file runs whatever images are in `build/`.
  Build all three first and look at the build's own output: a failed build leaves the old image in place, and the scenarios then pass or fail on firmware that is not the source.
- A scenario that shows the firmware doing something it should not is written to pass on what it does today, named `KNOWN GAP`, with a comment saying which assertion to turn round when the firmware is put right.
- For a long run use `runForMs()` and work out when it should end.
  `runUntilClockOutPulses()` builds the list of pulses again at every instruction: 64 steps took 14 s that way and 2.5 s the other.
- Do not let a scenario file grow past about 25,000 characters; start another by topic, with what the two share in a file beside them that is not named `*.test.js`, as `midi_fixture.js` is.
- What every scenario file shares is in `scenarios/fixture.js`: the figures (`READING_150_BPM`, `STEP_MS`, `EDGE_JITTER_MAX_MS` and the like), `noteMillivolts()`, `assertNear()`, `assertAllEqual()` and `nowMs()`.
  Require them from there, not declare them again, and loop over the board's `CONFIGS`.
  A figure or helper that differs from file to file (`PANEL`, `RUN_LIMIT_MS`, `SCAN_MS`, `playingBoard()`) stays in its file.
