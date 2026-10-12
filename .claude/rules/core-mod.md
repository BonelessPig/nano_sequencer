---
paths:
  - "core/mod.*"
  - "core/seq.*"
---

# core: modulation

Loaded with the files below, on top of `core/CLAUDE.md`, whose rules apply to all of `core/`.

- `mod` knows a signal, an amount and the depth the two make, and nothing of what the depth is for: `seq.c` says, with `mod_shift()` and `mod_chance()`, and the ranges and limits it passes are in `mod_config.h`.
  `mod` includes only `prng` and `mod_config.h`, so another board can take it as it is and write that one header for itself.
- `mod_config.h` is constants only and includes nothing: the reading's width in bits and the dead zone, which `mod` works the centre, the top and its scaling out from, and a range, a least and a most for each thing `seq.c` shifts.
  A new shift's numbers go there; `mod.c` refuses a width outside 9 to 15 bits at compile time.
  A shift of `1U` by more than 7 is a rule 12.2 finding, so the macros made from the width shift a `(uint16_t)1U`.
- The LFO and its amounts are a control like any other: in `seq_mod_t` inside `seq_inputs_t`, read when a step begins.
  An LFO that is not valid is taken to be at its centre.
- The centre is 512 and a reading within 16 of it is the centre too, as `mod_config.h` has them.
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
