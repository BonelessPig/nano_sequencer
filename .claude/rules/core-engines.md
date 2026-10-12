---
paths:
  - "core/engine*.*"
  - "core/seq.*"
---

# core: engines

Loaded with the files below, on top of `core/CLAUDE.md`, whose rules apply to all of `core/`.

- `engine.h` is the interface: a state type, `engine_<name>_init()` (which is also the reset) and `engine_<name>_step()`, taking `engine_inputs_t` and filling in `engine_step_t`.
  A new engine follows `engine_plain`.
- The sequencer owns the clock and decides when a step is due.
  An engine decides which step it is and its note value, and never touches time or pitch.
- An engine reads the panel only through `panel_step_value()`, and its own mode switches (3 to 7) through `panel_mode_value()`.
  The low three are the sequencer's; an engine never looks at them, and ignores any of the five it has no use for.
- The engines are numbered 0 to 5: plain, stage, Klee, chance, Cartesian, Euclidean.
  6 and 7 are free.
- Chance comes from the one generator in the sequencer's state, through `p_prng` in `engine_inputs_t`.
  An engine draws with `prng_byte()` and compares so that a draw of 0 means "leave it alone": a null generator gives 0, and then nothing is left to chance.
  Engine init never seeds it; only `seq_init()` does.
- A test of anything random uses the real generator from a seed whose first bytes are written in `test_prng.c`, and says in a comment which draws it expects.
  Do not fake the generator.
- An engine with no use for the addressing controls (Klee, Cartesian) does not check that pointer; its header says so.
- A step function that returns false must leave its state where it was.
- The selector between engines is a `switch` in `seq.c` on the low three mode switches as a number (`panel_mode_value()`); no function-pointer table.
  A number with no engine plays the plain one.
  A new engine takes the next number, a state of its own in `seq_state_t` beside the others (no union), a line in `engines_init()` and a case in `engine_step()`.
- The engine is chosen when a step begins, from the panel as last read, valid or not.
  A change of engine sends every engine back to its start, and so do a reset and a start.
- An engine is asked once for every step, and a step is always 24 pulses.
  An engine that makes something longer than a step (a stage of several steps) counts the steps itself and gives each one in turn: a silent one as a rest (note value 0), a held one as a note with `b_tie` set.
  It never asks the sequencer for a length.
- `b_tie` means the gate is still open when the next step begins.
  `seq.c` does that by giving the note a gate of a whole step if the gate length is shorter; the gate module knows nothing of ties.
  The stage engine leaves it off on the last step of a hold; the Klee engine's merge sets it on every note and lets the rest that follows end the gate at the boundary.
- An engine with fewer than sixteen places (the stage engine's eight) takes the addressing range modulo its own count and hands `address_next()` a range that does not wrap, then folds the step it gets back.
  Direction, pendulum and one-shot then need no code of their own.
- The stage engine's layout (which steps are controls, which switches are the length and which the mode, the order of the modes) is constants at the top of `engine_stage.c`, described in words in its header; the tests define their own.
