---
paths:
  - "core/transport.*"
  - "core/clock.*"
  - "core/debounce.*"
  - "core/seq.*"
---

# core: transport and the external clock

Loaded with the files below, on top of `core/CLAUDE.md`, whose rules apply to all of `core/`.

- `seq_tick()` runs the transport first, on every tick.
  A start (the first tick, and each time the run button starts the pattern) puts the clock, the engine and the gate back to their beginning and passes on no elapsed time for that tick, so the first step is a whole one.
  Do not let a start count its own tick.
- Stopped means nothing is due: the gate is closed on every stopped tick, the clock output is low, the pitch CV holds, and reset is not looked at.
  An external edge moves nothing there; it only sets which clock the next start is on.
- `pulses` is given on every tick and is the one thing that moves while stopped.
  Until an external edge comes, the stopped branch (`idle()` in `seq.c`) advances the internal clock for its pulses alone; it must never touch `step_pulses`, the engines or anything else a start does not put back.
  Once one has come the clock is left alone and no pulse falls.
- The clock chooses itself.
  `transport` latches the first external edge and holds it until a stop; it never goes back to the internal clock for want of edges.
  An edge is looked at while stopped too, for the latch alone, so that a start with a clock running waits for its edge.
  `b_external` in `transport_outputs_t` is that latch.
  It exists because `mode` does not say which clock a stopped pattern would start on.
- A control that bounces (the run button) arrives as read, and `transport` debounces it.
  `debounce` counts in ticks, like `cal`: it is for a finger, not for music.
  Its first reading is taken as it stands; `transport` starts with the button as if held, so a button down at power-on is not a press.
- An input that is an event (a clock edge, a reset edge) arrives as a flag that is true for one tick.
  The core never sees how long the pulse was.
- The tempo reading has two meanings, and `seq.c` owns both: a tempo on the internal clock, a ratio on the external one.
  The ratio's parts are constants at the top of `seq.c`.
- `clock_follow()` keeps an exact count: 24 times the multiply from one sync edge to the next, never more and never fewer, whatever the edges do.
  It holds one pulse back for the edge, so a step never begins ahead of its edge, and gives all that is left when the edge comes.
  Keep that property; it is what stops the pattern slipping against the clock.
  Steps owed after an early edge begin one per tick, like steps owed after a stall.
- The ratio is read at a sync edge and held until the next, like a control read when a step begins.
  A multiply needs a measured spacing, so the first stretch is always given as one step.
- `clock_advance()` ends any following, so `seq.c` needs no state of its own for a change of clock.
  `clock_follow()` is told how many pulses the sequencer has left before its next step, and its first sync edge gives exactly that, so the step begins on the edge.
  A start on the external clock leaves the step one pulse short of due for the same reason.
- A change of clock is not a start and must not restart the pattern.
  Going to the external clock, the step in progress ends on the edge that took over and the next begins there; the way back is a stop and a start.
- On the external clock a step can fall due while the clock output is still high from the step before (an edge in under half the last gap).
  It is held back one tick, with the gate closed and the clock output low, so that every step has a rising edge of its own.
  The internal clock is left alone: there it takes a stalled loop, and a late step begins at once, as before.
  A gate tied by its length is not broken by this, because the clock output has fallen by then.
