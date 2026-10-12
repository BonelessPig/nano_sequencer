---
paths:
  - "adapters/target/eeprom.*"
  - "adapters/target/reset_cause.*"
  - "adapters/target/atmega328p_eeprom_regs.h"
---

# adapters/target: EEPROM and reset cause

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- `eeprom.c` implements `nv_port.h` and owns the EEPROM registers; nothing else includes `atmega328p_eeprom_regs.h`.
  It is one file with no driver under it, like `timebase.c`, because the peripheral has one user: the crash log goes through `nv_port` from the app like the calibration record, so it is not a second one.
  Split the file if something in this folder needs the EEPROM.
- Nothing waits for the EEPROM.
  `nv_write()` only copies the bytes; `nv_poll()` starts at most one byte per call, and only when the hardware's write bit is clear.
  A byte that already holds the value is skipped.
- Starting a byte is a timed sequence: EEMPE, then EEPE within four clock cycles.
  Keep the two writes adjacent, with interrupts masked around just them and the status register restored as found, and check with `avr-objdump -d` that they compile to an `out` followed directly by an `sbi`.
  `EECR` needs `REG_IO_LOW` for that.
- A read while a write is in hand is refused, not waited for: it would move the address register.
- The calibration record is at address 0 and the crash log at 16 (the app's choices, not this file's).
  Another user of the EEPROM takes other addresses.
- `reset_cause.c` passes on the low four bits of `MCUSR` as they are, because the port gives the causes in that order, and clears the register.
  Read it once, at start-up.
  What it holds on the board depends on the bootloader (`docs/ROADMAP.md` section 8).
