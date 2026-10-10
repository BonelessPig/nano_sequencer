# adapters/target/ rules

The ports implemented for the ATmega328P. This is the only folder that touches hardware.

## Registers

- **Registers live in two files.** `atmega328p_regs.h` declares each register as `extern volatile`; `atmega328p_regs.ld` gives it its address and is passed to the linker as an extra input. A new register needs both. Use the plain datasheet data address (for example `0x2B`). A register missing from the `.ld` file fails at link time; a wrong address links silently, so check it against the datasheet.
- Registers at 0x20 to 0x3F need the `REG_IO_LOW` attribute or the compiler stops using `sbi`/`cbi`, which changes the shift register pulse timing.
- Putting the address inside the attribute does not work on avr-gcc 12.1.0 (`io_low(0x2B)` fails with "IO definition needs an address"), which is why the linker file exists.
- **A macro used by only one build configuration is a rule 2.5 finding in the other.** Definitions that only one configuration's files use go in a header only those files include. That is why the USART0 registers and bit positions are in `atmega328p_usart_regs.h` (same `.ld` file), included only by `serial_logger.c`.
- A new register also goes into `tests/fake_atmega328p_regs.h` and its `fake_regs_reset()`.

## Compiler

- **Compiler builtins need a visible prototype** or cppcheck reports rule 17.3. `delay.c` declares `__builtin_avr_delay_cycles` for that reason; avr-gcc accepts the redeclaration and still inlines it.
- The `io_low` attribute and the delay builtin are compiler extensions that cppcheck does not flag.

## Loggers and the UART

- `serial_logger.c` (debug) and `null_logger.c` (release) both implement `log_port.h` and `logger.h`. The Makefile links one. A change to either interface changes both files.
- The serial rate is 115200 baud in double-speed mode, which is 2.1 % fast at 16 MHz. 250000, 500000 and 1000000 are exact.
- The release build never enables the USART. PD0 and PD1 are not configured by `register_init.c`; leave them alone so the release build does not drive them against the board's USB serial chip.

## Pins

- In use: PD2 (74HC165 load), PD3 (clock), PD4 (data in), ADC channel 6 (tempo pot).
- PB5, PC0 and PC1 are given a direction but are unused and free.

## Testing and analysis

- Every `.c` file here has a host test that `#include`s it against fake registers, and must stay at 100% of lines and branches. `tests/CLAUDE.md` has the recipe.
- This folder gets more MISRA leeway than `core/`, because hardware access legitimately needs things like `volatile`. It currently has no deviations; `tools/misra/CLAUDE.md` says how to add one if it is truly needed.
- When a part or pin changes here, update its model or wiring in `tools/sim/` in the same change.
