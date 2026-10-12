---
paths:
  - "core/mod.*"
  - "core/seq.*"
---

# core: modulation

Loaded with the files below, on top of `core/CLAUDE.md`, whose rules apply to all of `core/`.

- `mod` knows a signal, an amount and the depth the two make, and nothing of what the depth is for: `seq.c` says, with `mod_shift()` and `mod_chance()`, and the ranges are constants at its top.
  `mod` includes only `prng`, so another board can take it as it is.
- The LFO and its amounts are a control like any other: in `seq_mod_t` inside `seq_inputs_t`, read when a step begins.
  An LFO that is not valid is taken to be at its centre.
- The centre is 512 and a reading within 16 of it is the centre too.
  That is what makes an input with nothing plugged in move nothing, whatever the amounts are; keep it.
- A chance is drawn only when it is not 0, and only for a step that holds a note.
  With no modulation the random numbers the engines draw are therefore what they were, and the engines' tests rely on it.
- The reading, the amount and the depth are each brought to 8 bits before they are multiplied, so every product fits in 16.
  `mod_chance()` adds the draw to the depth in 16 bits and compares the sum: written as a draw above `255 - depth` it passes the host build and fails avr-gcc, as `core/CLAUDE.md` says of a saturating add.
- Another thing for the LFO to move is an amount in `seq_mod_t`, a slot in `app.c` and a call in `seq.c`.
  `mod` is only touched if it needs a new kind of move.
