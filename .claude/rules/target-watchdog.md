---
paths:
  - "adapters/target/watchdog.*"
  - "adapters/target/atmega328p_wdt_regs.h"
  - "adapters/target/init.c"
---

# adapters/target: the watchdog

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- `watchdog.c` implements `watchdog_port.h` and owns `WDTCSR`; nothing else includes `atmega328p_wdt_regs.h`.
- Setting the timeout is two writes that must be within four clock cycles of each other.
  Keep them adjacent and check with `avr-objdump -d` that they are `ldi`, `sts`, `ldi`, `sts` with nothing between.
  They are not wrapped in an interrupt mask because `watchdog_init()` runs before `timebase_init()`; if it ever runs with interrupts on, mask them as `eeprom.c` does.
- `watchdog_init()` goes after everything in `platform_init()` that can fail, so a failed bring-up halts and is not reset for ever, and it must stay well within 16 ms of reset: after a watchdog reset the hardware has the watchdog running at that timeout already.
- It leaves `MCUSR` alone.
  The reset cause is read, and the flags cleared, by `reset_cause.c` afterwards.
- `wdr` comes from `__builtin_avr_wdr`, called inside a `static inline` function the fake header replaces (`adapters/target/CLAUDE.md`, under Compiler, has its prototype).
- The stock old bootloader does not turn the watchdog off, so on that bootloader a watchdog reset loops until the power is cycled (`docs/OPEN_ITEMS.md` item 15).
