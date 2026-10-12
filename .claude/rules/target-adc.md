---
paths:
  - "adapters/target/analog_reader.*"
---

# adapters/target: the ADC

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- `analog_reader.c` owns the ADC and implements three ports on it: the pot (`pot_input_port.h`, channel 6), the modulation input (`mod_input_port.h`, channel 7) and the LFO rate pot (`rate_input_port.h`, channel 0, PC0).
  Each read sets the channel itself, so neither depends on what the other left.
- A read waits for its conversion, about 0.1 ms, bounded by a poll budget.
  The two pots are read in the 8 ms scan and the modulation input on every other tick, so every tick has one conversion in it and a scan tick two; the outputs are applied before them, so no edge moves.
  Do not add a conversion to a tick without measuring: a third in the scan made the first tick after power-on run past its millisecond on the emulated board (step 0's gate rose up to 0.08 ms late), and one on a tick of its own put that tick's MIDI bytes up to 0.25 ms later.
  A conversion ahead of the panel read moves the panel read, which the release image's scan scenario catches.
- The modulation input's stage has to put a signal at its centre, or no signal at all, at half the reference: a reading of 512.
  The firmware takes anything within 16 of that as nothing.
  The stage is not designed (`docs/ROADMAP.md` section 8).
