---
paths:
  - "core/bank.*"
  - "core/pickup.*"
---

# core: the amount bank

Loaded with the files below, on top of `core/CLAUDE.md`, whose rules apply to all of `core/`.

- One pot sets eight amounts, and a selector says which.
  The bank gives the slots no meaning: the app names them, by selector position (`SLOT_*` in `app.c`), and the sequencer is handed what it uses.
- A slot the selector has just come to is not the pot's until the pot has met its amount (`pickup`): a reading within 8 of it, or two readings with it between them.
  So passing through a slot changes nothing, and neither does a selector that misreads between two positions.
  Keep it so: nothing but an attached pot writes an amount, save `bank_set()`, which is for an amount from somewhere else (a recalled profile) and detaches the pot if the slot is the selected one, so that the pot does not pull it straight back.
- The home slot takes the pot's first valid reading wherever the selector stands, so that the tempo starts where the pot is.
  Every other amount starts at 0, and the bank keeps nothing through a power cycle; the app's profiles do, and put amounts back with `bank_set()`.
- `bank_tick()` with the same inputs again changes nothing, so the app calls it on every tick with controls that are read every eighth.
