---
paths:
  - "core/cal.*"
  - "core/cal_record.*"
  - "core/pitch_cal.*"
---

# core: calibration

Loaded with the files below, on top of `core/CLAUDE.md`, whose rules apply to all of `core/`.

- `cal_tick()` runs on every tick, ahead of the sequencer.
  It chooses the mode once, on the first tick after `cal_init()`, and never changes its mind; outside the mode every output is false or 0 and the table is not touched.
- The mode's controls (the entry pattern, which step chooses the point, which switch stores, the trim range, the gate's low time) are constants at the top of `cal.c`.
  The header describes them in words, with the numbers, and the tests define their own.
- `cal` counts in ticks, not pulses: it has no clock, and its one time (the gate low after a store) is for a person, not for music.
- The core still does not know what a CV value is in volts: the nominal value of an octave is an input.
- A change to the layout of the record takes a new version number in `cal_record.c`, so that old records are refused and not misread.
  The checksum has a test vector worked out outside the firmware; keep one.
