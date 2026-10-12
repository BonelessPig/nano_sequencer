---
paths:
  - "tools/sim/scenarios/transport.test.js"
  - "tools/sim/parts/clock_source.js"
  - "tools/sim/parts/clock_patterns.js"
  - "tools/sim/parts/switch.js"
  - "tools/sim/lib/edge_flag.js"
  - "tools/sim/lib/duty_meter.js"
---

# tools/sim: the clock, the jacks, the run button and the level light

Loaded with the files below, on top of `tools/sim/CLAUDE.md`, whose rules apply to all of `tools/sim/`.

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
- The level light is pulse width modulated at 977 Hz, so it is measured, not recorded: `takeLevel()` gives the part of the time the pin was high since it was last called.
  Call it once to start a stretch, run for a whole number of periods or many of them (100 ms), and call it again.
  A level of n is high for n + 1 counts in 256, and a level of 0 is exactly 0.
