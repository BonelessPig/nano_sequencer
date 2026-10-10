# tools/sim/ rules

The emulated board behind `make sim`: each `build/<config>/output.hex` runs unmodified on an emulated ATmega328P (the avr8js library under Node), with the board's external parts modelled around it. This is JavaScript test tooling, outside the C coding standard; the line length limits still apply.

## Layout

- `lib/`: the chip. `parts/`: models of external chips. Neither may know anything about the sequencer, so they can be lifted out for another project.
- `boards/nano_sequencer.js`: which part is on which pin, plus the panel, pot and serial capture.
- `scenarios/*.test.js`: what each firmware image must do, using Node's built-in test runner. No other dependencies; avr8js is pinned in `package-lock.json`.

## Writing parts and scenarios

- A part is driven only through the wires that reach it and models the datasheet's logic, including which inputs are level-sensitive and which are edge-triggered. The first 74HC165 model got that wrong and blamed the firmware.
- Measure time in CPU cycles from `cpu.cycles`, never wall time, so every run gives the same result.
- Every run is bounded: use `runUntil` with a limit so a scenario fails instead of hanging.
- Time a step from the first byte of its log line (`startCycle`), not the last: the line is sent a byte per loop pass, so its end moves with its length.
- The release image has no log. Until there is a gate output, its scenarios can only show the tick rate (the panel scan period and the timer registers), not when a step begins.
- A peripheral the firmware starts using (a timer, SPI) has to be instantiated in `lib/machine.js` first; avr8js does nothing for registers nobody models.
- When a pin or part changes in `adapters/target/`, change the board and its scenarios in the same piece of work.

## Limits

The chip and parts are models. Nothing analog is modelled, parts have no setup or hold limits, and only the GPIO, ADC, USART and Timer/Counter2 models (with its compare match interrupt) have been exercised so far. A scenario passing is not proof on hardware; say so when reporting.
