---
paths:
  - "adapters/target/analog_reader.*"
---

# adapters/target: the ADC

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- `analog_reader.c` owns the ADC and implements two ports on it: the pot (`pot_input_port.h`, channel 6) and the modulation input (`mod_input_port.h`, channel 7).
  Each read sets the channel itself, so neither depends on what the other left.
- A read waits for its conversion, about 0.1 ms, bounded by a poll budget.
  The pot is read in the 8 ms scan and the modulation input on every tick, for the vibrato, so every tick has one conversion in it and a scan tick two; the outputs are applied before them, so no edge moves.
  The app reads the modulation input after the scan: read ahead of it, it made the first panel read about 0.1 ms early against the rest, which the release image's scan scenario caught.
- The modulation input's stage has to put a signal at its centre, or no signal at all, at half the reference: a reading of 512.
  The firmware takes anything within 16 of that as nothing.
  The stage is not designed (`docs/ROADMAP.md` section 8).
