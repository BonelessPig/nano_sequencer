# adapters/target/ rules

The ports implemented for the ATmega328P. This is the only folder that touches hardware.

## Registers

- **Registers live in two files.** `atmega328p_regs.h` declares each register as `extern volatile`; `atmega328p_regs.ld` gives it its address and is passed to the linker as an extra input. A new register needs both. Use the plain datasheet data address (for example `0x2B`). A register missing from the `.ld` file fails at link time; a wrong address links silently, so check it against the datasheet.
- Registers at 0x20 to 0x3F need the `REG_IO_LOW` attribute or the compiler stops using `sbi`/`cbi`, which changes the shift register load pulse.
- Putting the address inside the attribute does not work on avr-gcc 12.1.0 (`io_low(0x2B)` fails with "IO definition needs an address"), which is why the linker file exists.
- **A macro used by only one build configuration is a rule 2.5 finding in the other.** Definitions that only one configuration's files use go in a header only those files include. That is why the USART0 registers and bit positions are in `atmega328p_usart_regs.h` (same `.ld` file), included only by `serial_logger.c`.
- Group a new peripheral's registers in a header of its own on the same pattern (`atmega328p_timer2_regs.h`, `atmega328p_spi_regs.h`, `atmega328p_eeprom_regs.h`, `atmega328p_timer1_regs.h`, `atmega328p_extint_regs.h`), with only the bit positions the code uses, and check each position against the toolchain's `avr/iom328p.h`. An unused bit position is a rule 2.5 finding, and so is an unused mask in `bits.h`.
- A new register also goes into `tests/fake_atmega328p_regs.h` and its `fake_regs_reset()`.

## Drivers and devices

- Two levels. A **driver** runs one MCU peripheral and implements no port (`spi.c`); its header is private to this folder. A **device adapter** implements a port for one external part, using a driver (`shift_reg_reader.c` implements `step_input_port.h` on the SPI driver, and `cv_output.c` implements `cv_output_port.h` on it for the MCP4822).
- A driver owns its peripheral's registers and pins: only `spi.c` includes `atmega328p_spi_regs.h` or sets the direction of SCK, MOSI and SS. A device adapter owns the part's own lines (the 74HC165 load line, a chip select). SS is both: `spi.c` makes it a high output, which master mode needs, and `cv_output.c` then drives it as the DAC's chip select.
- A chip select is released on every path out of a write, failures included. A part that ignores a short frame (the MCP4822) is then left as it was.
- A part with no chip select (the 74HC165 chain) is clocked by every transfer on the bus. That is harmless only because it is loaded afresh before each read; keep it so.
- A driver call that waits is bounded by a poll budget and returns `ERR_TIMEOUT`, like the ADC read.
- The SPI is master, mode 0, MSB first, 1 MHz. Every part on the bus must work in that mode, or the mode becomes a parameter of the transfer. SS (PB2) stays an output: as a low input it cancels master mode.
- A pin that idles high is set high before it is made an output.
- A driver's init goes in `platform_init()` after `register_init()` and before `timebase_init()`.

## EEPROM and reset cause

- `eeprom.c` implements `nv_port.h` and owns the EEPROM registers; nothing else includes `atmega328p_eeprom_regs.h`. It is one file with no driver under it, like `timebase.c`, because the peripheral has one user. Split it if a second one arrives (the crash log).
- Nothing waits for the EEPROM. `nv_write()` only copies the bytes; `nv_poll()` starts at most one byte per call, and only when the hardware's write bit is clear. A byte that already holds the value is skipped.
- Starting a byte is a timed sequence: EEMPE, then EEPE within four clock cycles. Keep the two writes adjacent, with interrupts masked around just them and the status register restored as found, and check with `avr-objdump -d` that they compile to an `out` followed directly by an `sbi`. `EECR` needs `REG_IO_LOW` for that.
- A read while a write is in hand is refused, not waited for: it would move the address register.
- The calibration record is at address 0 (the app's choice, not this file's). Another user of the EEPROM takes other addresses.
- `reset_cause.c` passes on the low four bits of `MCUSR` as they are, because the port gives the causes in that order, and clears the register. Read it once, at start-up. What it holds on the board depends on the bootloader (`docs/ROADMAP.md` section 8).

## Edge inputs

- `clock_input.c` (PB0, Timer/Counter1's input capture flag) and `reset_input.c` (PD2, the flag of external interrupt 0) each poll a flag the MCU latches on an edge. Each owns its peripheral's registers, like `eeprom.c`, and has an init function that `platform_init()` calls after `register_init()` and before `timebase_init()`.
- The interrupts those flags belong to stay off, and each init writes the mask register to make sure. Neither vector has a handler: an enabled one would land in `__bad_interrupt` and restart the firmware.
- A flag is cleared by writing a one to it. Write the mask with a plain assignment (`TIFR1 = CAPTURE_FLAG;`), never `|=`: that reads the register and writes back a one for every flag that happens to be set, clearing them all.
- Clear the flag as the last step of init. Choosing the edge can set it.
- The edge is the falling one at the pin, because the input stage between jack and pin is taken to invert. If the stage built does not, change the edge in the two init functions and nowhere else.
- That a flag latches with its interrupt off has not been tried on the chip, and the emulator's model of it was written here (`docs/ROADMAP.md` section 8).
- `run_input.c` (PD7) and `sync_input.c` (PD3) read a pin that is low while the contact is closed. They debounce nothing; the core does.

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

- In use: PB1 (74HC165 load), PB5 (SCK, the clock of the chain and the DAC), PB4 (MISO, the chain's data, through 1 kΩ), PB3 (MOSI, the DAC's data), PB2 (SS, the DAC's chip select, idle high), PD4 (gate output), PD5 (clock output), ADC channel 6 (tempo pot), PB0 (clock input), PD2 (reset input), PD3 (sync switch), PD7 (run button). Timer/Counter2 is the 1 kHz tick and drives no pin, and the EEPROM has none either. Timer/Counter1 runs only for its capture flag.
- The MCP4822 is used on channel A at a gain of 2 from its internal reference (1 mV a step), with LDAC tied low. Its frame layout and pinout have been checked against the datasheet (`docs/ROADMAP.md` section 8).
- PD6 is free. PC0 and PC1 are given a direction but are unused and free.
- An input pin's direction and pull-up are set in `register_init.c` too. All four inputs use the MCU's own pull-ups.
- An output pin's direction and idle level are set in `register_init.c`, level first. The adapter that writes it (`gate_output.c`, `clock_output.c`) only changes its level, one pin per statement so that it compiles to a single `sbi` or `cbi` and cannot disturb the rest of the port. Check that with `avr-objdump -d` for a new one.
- The gate and clock outputs are active high. If a buffer stage inverts them, invert in those two files and nowhere else.
- PB3 to PB5 are also the ISP programmer's pins; the 1 kΩ on MISO is what lets a programmer override the chain.

## Testing and analysis

- Every `.c` file here has a host test that `#include`s it against fake registers, and must stay at 100% of lines and branches. `tests/CLAUDE.md` has the recipe.
- This folder gets more MISRA leeway than `core/`, because hardware access legitimately needs things like `volatile`. It has one inline suppression, a false positive (rule 8.7 on the tick interrupt handler), and no true deviations; `tools/misra/CLAUDE.md` says how to add one if it is truly needed.
- When a part or pin changes here, update its model or wiring in `tools/sim/` in the same change.
