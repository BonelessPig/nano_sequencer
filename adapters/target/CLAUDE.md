# adapters/target/ rules

The ports implemented for the ATmega328P. This is the only folder that touches hardware.

## Registers

- **Registers live in two files.** `atmega328p_regs.h` declares each register as `extern volatile`; `atmega328p_regs.ld` gives it its address and is passed to the linker as an extra input. A new register needs both. Use the plain datasheet data address (for example `0x2B`). A register missing from the `.ld` file fails at link time; a wrong address links silently, so check it against the datasheet.
- Registers at 0x20 to 0x3F need the `REG_IO_LOW` attribute or the compiler stops using `sbi`/`cbi`, which changes the shift register pulse timing.
- Putting the address inside the attribute does not work on avr-gcc 12.1.0 (`io_low(0x2B)` fails with "IO definition needs an address"), which is why the linker file exists.
- **A macro used by only one build configuration is a rule 2.5 finding in the other.** Definitions that only one configuration's files use go in a header only those files include. That is why the USART0 registers and bit positions are in `atmega328p_usart_regs.h` (same `.ld` file), included only by `serial_logger.c`.
- Group a new peripheral's registers in a header of its own on the same pattern (`atmega328p_timer2_regs.h`), with only the bit positions the code uses.
- A new register also goes into `tests/fake_atmega328p_regs.h` and its `fake_regs_reset()`.

## Compiler

- **Compiler builtins need a visible prototype** or cppcheck reports rule 17.3. No builtin is in use now; the removed `delay.c` declared `__builtin_avr_delay_cycles` for that reason, and avr-gcc accepted the redeclaration and still inlined it.
- The `io_low` and `signal` attributes are compiler extensions that cppcheck does not flag.

## Interrupts

- An ISR does the minimum (a counter, a flag, a ring buffer) and never calls into the core or a port.
- Each variable shared with an ISR is a file-static `volatile` in exactly one `.c` file and is reached from outside only through a port function. Keep it to one byte so a read is a single instruction; anything wider needs interrupts masked around the access.
- The handler's name and attribute come from macros in the peripheral's register header (`TIMER2_COMPA_ISR`, `TIMER2_COMPA_ISR_ATTR`). The fake register header redefines them, so the host test calls the handler as an ordinary function.
- A handler must have external linkage for the vector table to find it. cppcheck does not see the vector table (it is machine code in the startup object), so it reports rule 8.7 (advisory). That is a false positive; suppress it inline with the reason, as `timebase.c` does.
- Check a new vector's number against the datasheet and confirm with `avr-objdump -d` that the vector table entry jumps to the handler. A wrong number links silently.
- `timebase_init()` is the last step of `platform_init()` because it sets the global interrupt enable bit. Anything that must be set up before interrupts run goes before it.

## Loggers and the UART

- `serial_logger.c` (debug) and `null_logger.c` (release) both implement `log_port.h` and `logger.h`. The Makefile links one. A change to either interface changes both files.
- The serial logger never waits for the UART. A message is written into a 128-byte queue and `log_poll()` sends one byte per call when the transmitter is ready. A message that does not fit is dropped whole, so write every message between `begin_message()` and `end_message()`. Only `log_flush()` blocks.
- The serial rate is 115200 baud in double-speed mode, which is 2.1 % fast at 16 MHz. 250000, 500000 and 1000000 are exact.
- The release build never enables the USART. PD0 and PD1 are not configured by `register_init.c`; leave them alone so the release build does not drive them against the board's USB serial chip.

## Pins

- In use: PD2 (74HC165 load), PD3 (clock), PD4 (data in), ADC channel 6 (tempo pot). Timer/Counter2 is the 1 kHz tick and drives no pin.
- PB5, PC0 and PC1 are given a direction but are unused and free.

## Testing and analysis

- Every `.c` file here has a host test that `#include`s it against fake registers, and must stay at 100% of lines and branches. `tests/CLAUDE.md` has the recipe.
- This folder gets more MISRA leeway than `core/`, because hardware access legitimately needs things like `volatile`. It has one inline suppression, a false positive (rule 8.7 on the tick interrupt handler), and no true deviations; `tools/misra/CLAUDE.md` says how to add one if it is truly needed.
- When a part or pin changes here, update its model or wiring in `tools/sim/` in the same change.
