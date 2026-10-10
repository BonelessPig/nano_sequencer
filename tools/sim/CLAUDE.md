# tools/sim/ rules

The emulated board behind `make sim`: each `build/<config>/output.hex` runs unmodified on an emulated ATmega328P (the avr8js library under Node), with the board's external parts modelled around it. This is JavaScript test tooling, outside the C coding standard; the line length limits still apply.

## Layout

- `lib/`: the chip, and `pin_recorder.js` for watching an output pin. `parts/`: models of external chips. Neither may know anything about the sequencer, so they can be lifted out for another project.
- `boards/nano_sequencer.js`: which part is on which pin, plus the panel, pot, the DAC's frames, the gate and clock recorders and serial capture.
- `scenarios/*.test.js`: what each firmware image must do, using Node's built-in test runner. No other dependencies; avr8js is pinned in `package-lock.json`.

## Writing parts and scenarios

- A part on the SPI bus is the exception to the next rule for its clock and data: avr8js's SPI does not move its pins, so the board hands the part each transfer through `spi.onByte` and the part answers with a byte. Its other lines (load, chip select) are still wires. Assert the SPI mode in a scenario, because the model cannot object to a wrong one.
- The board gives every transfer to every part on the bus, as the wiring does. A part with a chip select takes the byte only while selected; the 74HC165 chain has none and is clocked by all of them, so its clock count per read includes the DAC frames sent before the next load.
- A part records what it was told (the MCP4822's frames, with the cycle each took effect) and the board exposes that; a scenario asks what the part held at a given cycle (`cvWriteBefore`) to check an order of events against a pin recorder.
- A part is driven only through the wires that reach it and models the datasheet's logic, including which inputs are level-sensitive and which are edge-triggered. The first 74HC165 model got that wrong and blamed the firmware.
- Measure time in CPU cycles from `cpu.cycles`, never wall time, so every run gives the same result.
- Every run is bounded: use `runUntil` with a limit so a scenario fails instead of hanging.
- Time a step from the first byte of its log line (`startCycle`), not the last: the line is sent a byte per loop pass, so its end moves with its length.
- The release image has no log. Time its steps from the clock output, which pulses on every step, and its notes from the gate. The same measurements work on the debug image, where they are far steadier than the log (0.003 ms against 0.25 ms).
- An output with nothing modelled on it gets a pin recorder on the board, not a listener in the scenario. It records the level at the instant the port register is written.
- The first step after reset is a tick short, and so are its gate and clock pulse. Leave pulse 0 out of a width or period loop and assert it on its own.
- A peripheral the firmware starts using (a timer, SPI) has to be instantiated in `lib/machine.js` first; avr8js does nothing for registers nobody models.
- When a pin or part changes in `adapters/target/`, change the board and its scenarios in the same piece of work.

## Limits

The chip and parts are models. Nothing analog is modelled, parts have no setup or hold limits, and only the GPIO, ADC, USART, Timer/Counter2 (with its compare match interrupt) and SPI models have been exercised so far. SPI clock edges are not modelled at all, and an output pin is a logic level with no voltage, rise time or load. A scenario passing is not proof on hardware; say so when reporting.
