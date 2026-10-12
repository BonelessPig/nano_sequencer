---
paths:
  - "adapters/target/timebase.*"
  - "adapters/target/atmega328p_timer2_regs.h"
  - "adapters/target/init.c"
---

# adapters/target: interrupts and the tick

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- An ISR does the minimum (a counter, a flag, a ring buffer) and never calls into the core or a port.
- Each variable shared with an ISR is a file-static `volatile` in exactly one `.c` file and is reached from outside only through a port function.
  Keep it to one byte so a read is a single instruction; anything wider needs interrupts masked around the access.
- The handler's name and attribute come from macros in the peripheral's register header (`TIMER2_COMPA_ISR`, `TIMER2_COMPA_ISR_ATTR`).
  The fake register header redefines them, so the host test calls the handler as an ordinary function.
- A handler must have external linkage for the vector table to find it. cppcheck does not see the vector table (it is machine code in the startup object), so it reports rule 8.7 (advisory).
  That is a false positive; suppress it inline with the reason, as `timebase.c` does.
- Check a new vector's number against the datasheet and confirm with `avr-objdump -d` that the vector table entry jumps to the handler.
  A wrong number links silently.
- `timebase_init()` is the last step of `platform_init()` because it sets the global interrupt enable bit.
  Anything that must be set up before interrupts run goes before it.
