# tools/sim/ rules

The emulated board behind `make sim`: each `build/<config>/output.hex` runs unmodified on an emulated ATmega328P (the avr8js library under Node), with the board's external parts modelled around it.
This is JavaScript test tooling, outside the C coding standard; the line length limits still apply.

## Layout

- `lib/`: the chip, `pin_recorder.js` for watching an output pin, `duty_meter.js` for one switched too fast to record, and `edge_flag.js` for a flag that latches an edge on an input pin.
  `parts/`: models of external chips and of things plugged in or pressed (`clock_source.js`, `switch.js`), with `clock_patterns.js` for the uneven trains a clock source can be given.
  Neither may know anything about the sequencer, so they can be lifted out for another project.
- `boards/nano_sequencer.js`: which part is on which pin, plus the panel, the mode switches (`setModeSwitches()`), the selector (`setSelector()`), the level light (`takeLevel()`), the modulation input (`setLfoReading()`), the step lights (`ledLatches`, `ledsAt()`), pot, the DAC's frames, the gate and clock recorders, the two input jacks and the run button, what the USART sent (the debug image's log sorted into lines in `serial`, the midi image's bytes and the messages they make in `midi`), the EEPROM's contents and the reset flags at power-on (`createBoard(config, { eeprom, resetFlags })`, the config one of `CONFIGS`: debug, release or midi), and for the watchdog `stopTick()`, the cycle of each reset (`resets`) and the crash log read back from the EEPROM (`crashLog()`).
  `boards/cal_record.js` works the calibration record out on its own, so a scenario does not check the firmware against itself, and `boards/crash_record.js` does the same for a crash log record, says where the log is, and reads a log back into its records (`readCrashLog()`).
  `boards/midi_messages.js` reads MIDI bytes into messages, from the MIDI specification and not from the firmware, and names a note.
  `boards/wiring.js` says which signal is on which pin of the Nano, and is the only place the tooling writes that down: the board wires its parts from it, and `docs/SCHEMATIC.html` loads the same file to label its chip drawings.
  A pin that moves is changed there; the hand-drawn figures in the schematic page still carry their own labels and are changed by hand.
- `scenarios/*.test.js`: what each firmware image must do, using Node's built-in test runner.
  No other dependencies; avr8js is pinned in `package-lock.json`.
- `live/`: `make live`, the board run at real speed and shown in a browser.
  A viewer, not a check; its rules are in `live/CLAUDE.md`.

## Writing parts and scenarios

- A part on the SPI bus is the exception to the next rule for its clock and data: avr8js's SPI does not move its pins, so the board hands the part each transfer through `spi.onByte` and the part answers with a byte.
  Its other lines (load, chip select) are still wires.
  Assert the SPI mode in a scenario, because the model cannot object to a wrong one.
- The board gives every transfer to every part on the bus, as the wiring does.
  A part with a chip select takes the byte only while selected; the 74HC165 chain has none and is clocked by all of them, so its clock count per read includes the DAC frames sent before the next load.
- A part records what it was told (the MCP4822's frames, with the cycle each took effect) and the board exposes that; a scenario asks what the part held at a given cycle (`cvWriteBefore`) to check an order of events against a pin recorder.
- A part is driven only through the wires that reach it and models the datasheet's logic, including which inputs are level-sensitive and which are edge-triggered.
  The first 74HC165 model got that wrong and blamed the firmware.
- Measure time in CPU cycles from `cpu.cycles`, never wall time, so every run gives the same result.
- Every run is bounded: use `runUntil` with a limit so a scenario fails instead of hanging.
- Time a step from the first byte of its log line (`startCycle`), not the last: the line is sent a byte per loop pass, so its end moves with its length.
- The release and midi images have no log.
  Time their steps from the clock output, which pulses on every step, and its notes from the gate.
  The same measurements work on the debug image, where they are far steadier than the log (0.003 ms against 0.25 ms).
- An output with nothing modelled on it gets a pin recorder on the board, not a listener in the scenario.
  It records the level at the instant the port register is written.
- A scenario goes in the file for its topic, inside that file's loop over the three images if it should hold for every one: `transport.test.js`, `calibration.test.js`, `engines.test.js` (the mode switches or an engine), `watchdog.test.js`, `modulation.test.js`, and `midi.test.js` or `midi_clock.test.js` for the midi image alone.
- The board sorts the log: `serial.lines` holds the step lines, which scenarios count and time, and `serial.notices` every other line (start-up, calibration, errors).
  A new kind of line goes to the notices unless it begins `Step `.
- There is one wire from the USART and the board knows from its config what is on it.
  A new image is a line in `WIRES` in the board.
- A power cycle is a second board created with the first one's `eepromBytes()`.
  A scenario with no log (the release and midi images, or calibration mode in any) runs for a time with `runForMs()`, not until a line.
- The chain is ten chips, so a panel read is 80 clocks and ten transfers.
  The selector is at 0 on a new board, so its pot sets the tempo, and a reading set after that changes the tempo as it always did; turned to another position, the pot has to meet what that position holds before it changes anything.
  The mode switches are off on a new board, which is the plain engine, so a scenario that does not set them plays sixteen steps.
- `node --test` on one scenario file runs whatever images are in `build/`.
  Build all three first and look at the build's own output: a failed build leaves the old image in place, and the scenarios then pass or fail on firmware that is not the source.
- A scenario that shows the firmware doing something it should not is written to pass on what it does today, named `KNOWN GAP`, with a comment saying which assertion to turn round when the firmware is put right.
- For a long run use `runForMs()` and work out when it should end.
  `runUntilClockOutPulses()` builds the list of pulses again at every instruction: 64 steps took 14 s that way and 2.5 s the other.
- Do not let a scenario file grow past about 25,000 characters; start another by topic, with what the two share in a file beside them that is not named `*.test.js`, as `midi_fixture.js` is.
- A peripheral the firmware starts using (a timer, SPI, the EEPROM) has to be instantiated in `lib/machine.js` first; avr8js does nothing for registers nobody models.

## Topic rules

The lessons about one kind of scenario are in `.claude/rules/`, each loaded when a file in its `paths:` list is read.
A new lesson about one of these goes in its file; one about the whole folder goes here.
Read the file yourself before writing a new file that belongs to its topic.

- `sim-transport.md`: the clock, the jacks, the run button and the level light (`transport.test.js`, `clock_source.js`, `clock_patterns.js`, `switch.js`, `edge_flag.js`, `duty_meter.js`).
- `sim-engines.md`: the engines and the step lights (`engines.test.js`, `hc595.js`).
- `sim-watchdog.md`: the watchdog and the crash log (`watchdog.test.js`, `crash_record.js`).
- `sim-midi.md`: MIDI out (`midi.test.js`, `midi_clock.test.js`, `midi_fixture.js`, `midi_messages.js`).
- `sim-modulation.md`: modulation (`modulation.test.js`).

## Limits

The chip and parts are models.
Nothing analog is modelled, parts have no setup or hold limits, and only the GPIO, ADC, USART (sending only), Timer/Counter2 (with its compare match interrupt), SPI, EEPROM and watchdog models have been exercised so far.
The two edge flags are this project's own model, with no synchroniser, noise canceller or minimum pulse width, and a switch bounces only as scripted.
The EEPROM model keeps the four-cycle rule for starting a write but never wears or fails, and the reset flags register holds only what the scenario put there.
SPI clock edges are not modelled at all, and an output pin is a logic level with no voltage, rise time or load.
The USART's transmit pin is not a pin here at all: what is captured is each byte as it is handed over, so bit timing, framing and the MIDI current loop are not modelled, and nothing is ever received.
The emulated USART holds one byte where the real one has a buffer behind its shift register, so on the chip a second byte is taken at once and the queue empties a byte sooner; a byte behind another leaves the pin there with no wait for the loop to come round, a few microseconds earlier than here.
A scenario passing is not proof on hardware; say so when reporting.
