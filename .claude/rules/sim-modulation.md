---
paths:
  - "tools/sim/scenarios/modulation.test.js"
---

# tools/sim: modulation

Loaded with the files below, on top of `tools/sim/CLAUDE.md`, whose rules apply to all of `tools/sim/`.

- A modulation scenario goes in `modulation.test.js`, inside the loop over the three images.
  Give a position its amount as the panel would (`setAmount()`: the selector, the pot to 0, the pot up, a scan after each), set the LFO, let a scan take it, and only then look at what plays (`playWithLfo()`).
  The LFO reads 512 on a new board, which moves nothing.
- The envelope CV is written a tick after the core works it out, in the same tick as the gate's edge and after it.
  Ask for it a whole millisecond after a gate edge (`envMillivoltsAt(rise + ms(1))`), and n milliseconds after the rise for the level n ticks into a slope.
