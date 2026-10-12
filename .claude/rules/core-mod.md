---
paths:
  - "core/mod.*"
  - "core/seq.*"
---

# core: modulation

Loaded with the files below, on top of `core/CLAUDE.md`, whose rules apply to all of `core/`.

- `mod` knows a signal, an amount and the depth the two make, and nothing of what the depth is for: `seq.c` says, with `mod_shift()` and `mod_chance()`, and the ranges are constants at its top.
  `mod` includes only `prng`, so another board can take it as it is.
- The LFO `seq` is handed is already the sum of the modulation input and the sequencer's own (`lfo_tick()`, which the app calls first), and is always valid from the app; `seq` and `mod` know nothing of that.
  `lfo.c` makes a signal as `mod.h` describes one, 10 bits about 512: if the signal's width becomes a board's choice, `lfo.c` has to follow it.
- The LFO and its amounts are a control like any other: in `seq_mod_t` inside `seq_inputs_t`, read when a step begins, but for the vibrato.
  An LFO that is not valid is taken to be at its centre.
- The centre is 512 and a reading within 16 of it is the centre too.
  That is what makes an input with nothing plugged in move nothing, whatever the amounts are; keep it.
- A chance is drawn only when it is not 0, and only for a step that holds a note.
  With no modulation the random numbers the engines draw are therefore what they were, and the engines' tests rely on it.
- The reading, the amount and the depth are each brought to 8 bits before they are multiplied, so every product fits in 16.
  `mod_chance()` adds the draw to the depth in 16 bits and compares the sum: written as a draw above `255 - depth` it passes the host build and fails avr-gcc, as `core/CLAUDE.md` says of a saturating add.
- Another thing for the LFO to move is an amount in `seq_mod_t`, a slot in `app.c` and a call in `seq.c`.
  `mod` is only touched if it needs a new kind of move.
- The amplitude is the one thing the LFO moves that `seq.c` only hands on: it is an output given with each step, 255 less the depth while the LFO is below its centre, and `midi_out` (the velocity) and `env` (the peak) say what it means.
  The envelope's time, at selector position 6, is an amount with no LFO in it and never goes near `seq`: the app hands it from the bank to `env_tick()`.
- The step position is the one move an engine makes and not `seq.c`: `engine_step()` works out the depth and hands it on as `offset` in `engine_inputs_t`, two plain values.
  `engine.h` does not include `mod.h`, and an engine never sees the LFO or the amount.
- The vibrato is the one move made on every tick, in `vibrato_cv()` as the outputs are filled in: the value given is the last note's moved towards the table's value for the pitch `MOD_VIBRATO_RANGE` semitones off, by `mod_scale()`.
  It never writes the state: `cv` and `semitone` there are the note's own.
  `mod_scale()` takes the span as its whole 255s and the rest, because a span times a depth passes 16 bits.
- The ranges at the top of `seq.c` (`MOD_*_RANGE`, `MOD_*_MIN`) are a board's choices and are on their way to a configuration header of their own; a new one goes beside them, under the same kind of name, and is never written as a number further down.
