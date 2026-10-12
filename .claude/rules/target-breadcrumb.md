---
paths:
  - "adapters/target/breadcrumb.*"
---

# adapters/target: the breadcrumb

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- `breadcrumb.c` implements `breadcrumb_port.h` with one byte marked `RAM_NOINIT` (in `atmega328p_regs.h`), which puts it in the `.noinit` section: the startup code neither clears nor sets it, so it keeps its value through a reset with the power on.
- Do not give it an initialiser; that would move it to `.data` and the startup code would set it on every start.
- Check with `avr-objdump -h` that the image has a `.noinit` section of one byte, and with `avr-nm` that the byte sits at `__bss_end`, past what the startup code clears.
- It is not `volatile` and no interrupt touches it.
  After power-on it holds anything; what it means is the app's business.
- The fake register header defines `RAM_NOINIT` as nothing.
  That the byte survives a reset is shown on the emulated board (`watchdog.test.js`), not by the host test.
