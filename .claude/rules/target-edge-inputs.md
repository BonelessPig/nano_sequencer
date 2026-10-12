---
paths:
  - "adapters/target/clock_input.*"
  - "adapters/target/reset_input.*"
  - "adapters/target/run_input.*"
  - "adapters/target/atmega328p_timer1_regs.h"
  - "adapters/target/atmega328p_extint_regs.h"
---

# adapters/target: the edge inputs and the run button

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- `clock_input.c` (PB0, Timer/Counter1's input capture flag) and `reset_input.c` (PD2, the flag of external interrupt 0) each poll a flag the MCU latches on an edge.
  Each owns its peripheral's registers, like `eeprom.c`, and has an init function that goes in `platform_init()` where a driver's does.
- The interrupts those flags belong to stay off, and each init writes the mask register to make sure.
  Neither vector has a handler: an enabled one would land in `__bad_interrupt` and restart the firmware.
- A flag is cleared by writing a one to it.
  Write the mask with a plain assignment (`TIFR1 = CAPTURE_FLAG;`), never `|=`: that reads the register and writes back a one for every flag that happens to be set, clearing them all.
- Clear the flag as the last step of init.
  Choosing the edge can set it.
- The edge is the falling one at the pin, because the input stage between jack and pin is taken to invert.
  If the stage built does not, change the edge in the two init functions and nowhere else.
- That a flag latches with its interrupt off has not been tried on the chip, and the emulator's model of it was written here (`docs/ROADMAP.md` section 8).
- `run_input.c` (PD7) reads a pin that is low while the contact is closed.
  It debounces nothing; the core does.
