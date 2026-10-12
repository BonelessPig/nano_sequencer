---
paths:
  - "tools/sim/scenarios/engines.test.js"
  - "tools/sim/parts/hc595.js"
---

# tools/sim: the engines and the step lights

Loaded with the files below, on top of `tools/sim/CLAUDE.md`, whose rules apply to all of `tools/sim/`.

- An engine's gates are checked against the clock output, which pulses on every step whatever the engine does: say which step each gate should rise on and how long it should be, and read its note from the DAC.
- The step lights change only at a panel read, so a light is up to a scan (8 ms) behind its step.
  Ask what they showed a scan and a half after an edge (`ledsAt()`), and run a scan or two past the last edge asked about.
  `ledLatches` has an entry per read, changed or not.
