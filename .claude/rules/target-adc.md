---
paths:
  - "adapters/target/analog_reader.*"
---

# adapters/target: the ADC

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- `analog_reader.c` owns the ADC and implements two ports on it: the pot (`pot_input_port.h`, channel 6) and the modulation input (`mod_input_port.h`, channel 7).
  Each read sets the channel itself, so neither depends on what the other left.
- A read waits for its conversion, about 0.1 ms, bounded by a poll budget.
  Both are read in the 8 ms scan, one after the other, so a scan tick is about 0.2 ms longer than the rest; the outputs are applied before it, so no edge moves.
  Something read on every tick would add its conversion to every tick.
- The modulation input's stage has to put a signal at its centre, or no signal at all, at half the reference: a reading of 512.
  The firmware takes anything within 16 of that as nothing.
  The stage is not designed (`docs/ROADMAP.md` section 8).
