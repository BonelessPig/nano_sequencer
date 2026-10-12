# tools/sim/ rules

The emulated board behind `make sim`: each `build/<config>/output.hex` runs unmodified on an emulated ATmega328P (the avr8js library under Node), with the board's external parts modelled around it.
This is JavaScript test tooling, outside the C coding standard; the line length limits still apply.

## Layout

- `lib/`: the chip, `pin_recorder.js` for watching an output pin, `duty_meter.js` for one switched too fast to record, and `edge_flag.js` for a flag that latches an edge on an input pin.
  `parts/`: models of external chips and of things plugged in or pressed (`clock_source.js`, `switch.js`), with `clock_patterns.js` for the uneven trains a clock source can be given.
  Neither may know anything about the sequencer, so they can be lifted out for another project.
- `boards/nano_sequencer.js`: which part is on which pin, and everything a scenario does to the board or reads from it, which `.claude/rules/sim-scenarios.md` lists.
  `boards/cal_record.js` works the calibration record out on its own, so a scenario does not check the firmware against itself, and `boards/crash_record.js` does the same for a crash log record, says where the log is, and reads a log back into its records (`readCrashLog()`).
  `boards/midi_messages.js` reads MIDI bytes into messages, from the MIDI specification and not from the firmware, and names a note.
  `boards/wiring.js` says which signal is on which pin of the Nano, and is the only place the tooling writes that down: the board wires its parts from it, and `docs/SCHEMATIC.html` loads the same file to label its chip drawings.
  A pin that moves is changed there; the hand-drawn figures in the schematic page still carry their own labels and are changed by hand.
- `scenarios/*.test.js`: what each firmware image must do, using Node's built-in test runner.
  No other dependencies; avr8js is pinned in `package-lock.json`.
- `live/`: `make live`, the board run at real speed and shown in a browser.
  A viewer, not a check; its rules are in `live/CLAUDE.md`.

## Writing parts

- A part on the SPI bus is the exception to the next rule for its clock and data: avr8js's SPI does not move its pins, so the board hands the part each transfer through `spi.onByte` and the part answers with a byte.
  Its other lines (load, chip select) are still wires.
  Assert the SPI mode in a scenario, because the model cannot object to a wrong one.
- The board gives every transfer to every part on the bus, as the wiring does.
  A part with a chip select takes the byte only while selected; the 74HC165 chain has none and is clocked by all of them, so its clock count per read includes the DAC frames sent before the next load.
- A part records what it was told (the MCP4822's frames, with the cycle each took effect) and the board exposes that; a scenario asks what the part held at a given cycle (`cvWriteBefore`) to check an order of events against a pin recorder.
- A part is driven only through the wires that reach it and models the datasheet's logic, including which inputs are level-sensitive and which are edge-triggered.
  The first 74HC165 model got that wrong and blamed the firmware.
- Measure time in CPU cycles from `cpu.cycles`, never wall time, so every run gives the same result.
- An output with nothing modelled on it gets a pin recorder on the board, not a listener in the scenario.
  It records the level at the instant the port register is written.
- A peripheral the firmware starts using (a timer, SPI, the EEPROM) has to be instantiated in `lib/machine.js` first; avr8js does nothing for registers nobody models.

## Topic rules

The lessons about one kind of scenario are in `.claude/rules/`, each loaded when a file in its `paths:` list is read.
A new lesson about one of these goes in its file; one about the whole folder goes here.
Read the file yourself before writing a new file that belongs to its topic.

- `sim-scenarios.md`: writing any scenario: what the board offers, timing, where a scenario goes (`scenarios/*.js`, `nano_sequencer.js`).
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
