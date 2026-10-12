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
- The level light is pulse width modulated at 977 Hz, so it is measured, not recorded: `takeLevel()` gives the part of the time the pin was high since it was last called.
  Call it once to start a stretch, run for a whole number of periods or many of them (100 ms), and call it again.
  A level of n is high for n + 1 counts in 256, and a level of 0 is exactly 0.
- An output with nothing modelled on it gets a pin recorder on the board, not a listener in the scenario.
  It records the level at the instant the port register is written.
- The first step is as long as the rest, after reset and after the run button.
- The firmware powers on running from its own clock, and the first edge on the clock input takes it over.
  A scenario that wants it waiting on the external clock, with nothing played, calls `startOnExternalClock()`: a stop, an edge and a start, with the records up to the start forgotten.
  Nothing in such a scenario can be timed from reset.
- An input pin whose edge the firmware polls from a flag (PB0, PD2) is driven through `machine.inputCapture1.setPin()` or `machine.externalInt0.setPin()`, never the port's own `setPin()`, or the flag is not set. avr8js has no input capture event and sets an external interrupt's flag only while the interrupt is enabled; `lib/edge_flag.js` fills that in from the datasheet.
  A scenario built on it shows what the firmware does with the flag, not that a chip sets it.
- The board speaks in jack levels.
  Each jack reaches its pin through an inverting stage, so `pulseClockIn()` is a rising edge at the jack and a falling one at the pin.
  `clockIn.rises` has the cycle of every jack edge, to time a step against.
  `startClockIn(periodMs, pattern)` takes one of `CLOCK_PATTERNS` for an uneven clock; a random one is the same train on every run, from its seed.
- A step begins between one and two ticks after its clock edge: taken at the next tick, applied at the one after.
  Assert that as a range.
  For a tight tolerance on a period, use a clock whose period is a whole number of milliseconds, so that every edge falls at the same point in its tick.
- A scenario that needs to know which step played, on an image with no log, reads the voltage the DAC held as each gate rose (`stepsPlayed()` in `transport.test.js`).
  Use a panel with no rests and distinct notes for it.
- `pressRunButton()` runs the chip for as long as the button is held, so it returns after the release.
  A press counts 20 ms after the contact settles; hold it for longer than that plus the bounce.
- A transport scenario that should hold for every image goes in `transport.test.js`, inside the loop over the three.
- The board sorts the log: `serial.lines` holds the step lines, which scenarios count and time, and `serial.notices` every other line (start-up, calibration, errors).
  A new kind of line goes to the notices unless it begins `Step `.
- A power cycle is a second board created with the first one's `eepromBytes()`.
  A scenario with no log (the release and midi images, or calibration mode in any) runs for a time with `runForMs()`, not until a line.
- A calibration scenario that should hold for every image goes in `calibration.test.js`, inside the loop over the three.
- A scenario about the mode switches or an engine goes in `engines.test.js`, inside the loop over the three.
  An engine's gates are checked against the clock output, which pulses on every step whatever the engine does: say which step each gate should rise on and how long it should be, and read its note from the DAC.
- The chain is ten chips, so a panel read is 80 clocks and ten transfers.
  The selector is at 0 on a new board, so its pot sets the tempo, and a reading set after that changes the tempo as it always did; turned to another position, the pot has to meet what that position holds before it changes anything.
  The mode switches are off on a new board, which is the plain engine, so a scenario that does not set them plays sixteen steps.
- The step lights change only at a panel read, so a light is up to a scan (8 ms) behind its step.
  Ask what they showed a scan and a half after an edge (`ledsAt()`), and run a scan or two past the last edge asked about.
  `ledLatches` has an entry per read, changed or not.
- The firmware never hangs by itself.
  A watchdog scenario stops the tick from outside with `stopTick()`, which clears the interrupt enable bit in `SREG`, and goes in `watchdog.test.js`, inside the loop over the three if it should hold for every image.
  Call it at a pin edge or a log line, which the loop makes: inside the tick interrupt it does nothing.
  The emulator's watchdog reset restarts the program and sets the reset flag and nothing else; `lib/machine.js` puts the USART back as well, and lists the reset's cycle.
  From its second reset on the emulator's watchdog asks twice, an instruction apart, which the list counts as one.
  Parts waiting on the chip's clock (a clock source, a bouncing switch) carry on through the reset.
  Do not read anything about the chip's registers after a reset from such a scenario.
  The RAM is another matter: the emulator's reset leaves it alone, as a real watchdog reset does, and the crash log scenarios rely on that for the breadcrumb.
  A fresh chip's RAM is zeros and the firmware never writes a place of 0, so a record with a place in it shows the byte was kept.
- After a watchdog reset give the firmware 25 ms before reading the crash log from the EEPROM: a record is five bytes at 3.6 ms each.
  On the debug image the first step's log line is up to a millisecond later than the rest, behind the start-up line, and several more when the crash log has a record to report; time a first step from the pins.
- There is one wire from the USART and the board knows from its config what is on it: text for the debug image, which it sorts into lines, and MIDI for the midi image, which it keeps as bytes (`midi.bytes`) and reads into messages as they come (`midi.messages`).
  `serial.bytes` counts every byte whatever it was.
  A new image is a line in `WIRES` in the board.
- A MIDI scenario goes in `midi.test.js`.
  A byte's cycle is the one in which the firmware handed it to the USART, when its start bit begins, and a message's is that of its first byte.
  Time a message from the pin edge it belongs to (a note from its gate, a step's first clock from the clock output), as a range: it waits for the bytes ahead of it at 320 µs each.
- Count MIDI clocks between one clock output pulse and the next (six, the first of them the step's own), not over a whole run: where a run ends decides how many of the last step's have gone.
  On the external clock the first edge gets one step whatever the ratio, so a clock multiplied by four gives 1 + 4 × (edges − 1) steps once it has stopped and played out.
- Check a note number twice over, against the panel and against the voltage the DAC held as the gate rose.
  Play the note messages back as a receiver would (`soundingAtEnd()` in `midi.test.js`) to show none is left on.
- `midi_messages.js` does not accept running status and calls anything but Start, Stop, the clock and the two note messages `unknown`.
  A scenario that asserts none is unknown has shown that no byte was dropped from inside a message.
- A modulation scenario goes in `modulation.test.js`, inside the loop over the three images.
  Give a position its amount as the panel would (`setAmount()`: the selector, the pot to 0, the pot up, a scan after each), set the LFO, let a scan take it, and only then look at what plays (`playWithLfo()`).
  The LFO reads 512 on a new board, which moves nothing.
- `node --test` on one scenario file runs whatever images are in `build/`.
  Build all three first and look at the build's own output: a failed build leaves the old image in place, and the scenarios then pass or fail on firmware that is not the source.
- A scenario that shows the firmware doing something it should not is written to pass on what it does today, named `KNOWN GAP`, with a comment saying which assertion to turn round when the firmware is put right.
- For a long run use `runForMs()` and work out when it should end.
  `runUntilClockOutPulses()` builds the list of pulses again at every instruction: 64 steps took 14 s that way and 2.5 s the other.
- A peripheral the firmware starts using (a timer, SPI, the EEPROM) has to be instantiated in `lib/machine.js` first; avr8js does nothing for registers nobody models.

## Limits

The chip and parts are models.
Nothing analog is modelled, parts have no setup or hold limits, and only the GPIO, ADC, USART (sending only), Timer/Counter2 (with its compare match interrupt), SPI, EEPROM and watchdog models have been exercised so far.
The two edge flags are this project's own model, with no synchroniser, noise canceller or minimum pulse width, and a switch bounces only as scripted.
The EEPROM model keeps the four-cycle rule for starting a write but never wears or fails, and the reset flags register holds only what the scenario put there.
SPI clock edges are not modelled at all, and an output pin is a logic level with no voltage, rise time or load.
The USART's transmit pin is not a pin here at all: what is captured is each byte as it is handed over, so bit timing, framing and the MIDI current loop are not modelled, and nothing is ever received.
The emulated USART holds one byte where the real one has a buffer behind its shift register, so on the chip a second byte is taken at once and the queue empties a byte sooner; a byte behind another leaves the pin there with no wait for the loop to come round, a few microseconds earlier than here.
A scenario passing is not proof on hardware; say so when reporting.
