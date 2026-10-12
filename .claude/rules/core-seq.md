---
paths:
  - "core/seq.*"
  - "core/gate.*"
---

# core: the sequencer's outputs and gate

Loaded with the files below, on top of `core/CLAUDE.md`, whose rules apply to all of `core/`.

- The step lights (`leds`) are the lights of the step that last began, given while the clock output is high and 0 otherwise.
  An engine with more to show than its step (the Klee register) has it read from its state in `step_leds()` in `seq.c`; do not add a field to `engine_step_t` for one engine.
- Outputs that are levels (`b_gate`, `b_clock_out`, `cv`, `leds`, `transport`) are given on every tick, as the level after that tick.
  Outputs that describe a step are given on the tick it begins and are zero otherwise.
- Musical lengths are counted in clock pulses, never ticks, so they scale with the tempo.
  When a step begins late, what it starts is counted from when it was due (`step_pulses` after the step's 24 are taken off), not from the tick that noticed.
- The pitch CV value changes only when a note begins, on the tick its gate opens, and holds through everything else (rests, unreadable steps, reset, a finished one-shot, a stop and a start).
  The core gives it as a number from 0 to 4095 and does not know what that is in volts.
- Every step that falls due calls `gate_begin()`, note or not: that is what ends a tied gate at a rest or at the end of a one-shot pattern.
  Look at the gate's level only after it.
