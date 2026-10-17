# adapters/target/ rules

The ports implemented for the ATmega328P. This is the only folder that touches hardware.

## Registers

- **Registers live in two files.** `atmega328p_regs.h` declares each register as `extern volatile`; `atmega328p_regs.ld` gives it its address and is passed to the linker as an extra input.
  A new register needs both.
  Use the plain datasheet data address (for example `0x2B`).
  A register missing from the `.ld` file fails at link time; a wrong address links silently, so check it against the datasheet.
- Registers at 0x20 to 0x3F need the `REG_IO_LOW` attribute or the compiler stops using `sbi`/`cbi`, which changes the shift register load pulse.
- Putting the address inside the attribute does not work on avr-gcc 12.1.0 (`io_low(0x2B)` fails with "IO definition needs an address"), which is why the linker file exists.
- **A macro used by only one build configuration is a rule 2.5 finding in the other.** Definitions that only one configuration's files use go in a header only those files include.
  That is why the USART0 registers and bit positions are in `atmega328p_usart_regs.h` (same `.ld` file), included only by `serial_logger.c` and `midi_output.c`, which are never linked together.
  Each of the two must use every macro in it, or the other's run reports rule 2.5: that is why the MIDI adapter enables the receiver (`RXEN0`) it does not read.
  A bit only one of them needs means splitting the header: `atmega328p_usart_rx_regs.h` has the receive bits and vector, for `midi_input.c` alone.
- Group a new peripheral's registers in a header of its own on the same pattern (`atmega328p_timer2_regs.h`, `atmega328p_spi_regs.h` and the rest), with only the bit positions the code uses, and check each position against the toolchain's `avr/iom328p.h`.
  An unused bit position is a rule 2.5 finding, and so is an unused mask in `bits.h`.
- A new register also goes into `tests/fake_atmega328p_regs.h` and its `fake_regs_reset()`.

## Drivers and devices

- Two levels.
  A **driver** runs one MCU peripheral and implements no port (`spi.c`); its header is private to this folder.
  A **device adapter** implements a port for one external part, using a driver (`shift_reg_reader.c` implements `step_input_port.h` on the SPI driver, and `cv_output.c` implements `cv_output_port.h` on it for the MCP4822).
- A driver owns its peripheral's registers and pins: only `spi.c` includes `atmega328p_spi_regs.h` or sets the direction of SCK, MOSI and SS.
  A device adapter owns the part's own lines (the 74HC165 load line, a chip select).
  SS is both: `spi.c` makes it a high output, which master mode needs, and `cv_output.c` then drives it as the DAC's chip select.
- A driver call that waits is bounded by a poll budget and returns `ERR_TIMEOUT`, like the ADC read.
- A pin that idles high is set high before it is made an output.
- A driver's init goes in `platform_init()` after `register_init()` and before `timebase_init()`.

## Compiler

- **Compiler builtins need a visible prototype** or cppcheck reports rule 17.3.
  One is in use, `__builtin_avr_wdr`, declared in the register header; avr-gcc accepts the redeclaration and still inlines it.
- The `io_low`, `signal` and `section` attributes are compiler extensions that cppcheck does not flag.

## Testing and analysis

- Every `.c` file here has a host test that `#include`s it against fake registers; `tests/CLAUDE.md` has the recipe.
- This folder gets more MISRA leeway than `core/`, because hardware access legitimately needs things like `volatile`.
  It has two inline suppressions of one false positive (rule 8.7 on the two interrupt handlers), and no true deviations; `tools/misra/CLAUDE.md` says how to add one if it is truly needed.
- When a part or pin changes here, update its model, the board and its scenarios in `tools/sim/` in the same change.

## Topic rules

The lessons about one peripheral or part are in `.claude/rules/`, each loaded when a file in its `paths:` list is read.
A new lesson about one of these goes in its file; one about the whole folder goes here.
Read the file yourself before writing a new file that belongs to its topic: `target-pins.md` before taking a pin, `target-interrupts.md` before adding a handler.

- `target-spi.md`: the SPI bus and the parts on it (`spi.*`, `shift_reg_reader.*`, `cv_output.*`, `atmega328p_spi_regs.h`).
- `target-eeprom.md`: EEPROM and reset cause (`eeprom.*`, `reset_cause.*`, `atmega328p_eeprom_regs.h`).
- `target-breadcrumb.md`: the breadcrumb (`breadcrumb.*`).
- `target-watchdog.md`: the watchdog (`watchdog.*`, `atmega328p_wdt_regs.h`, `init.c`).
- `target-edge-inputs.md`: the edge inputs and the run button (`clock_input.*`, `reset_input.*`, `run_input.*`, `atmega328p_timer1_regs.h`, `atmega328p_extint_regs.h`).
- `target-adc.md`: the ADC (`analog_reader.*`).
- `target-level.md`: the level light (`level_output.*`, `atmega328p_timer0_regs.h`).
- `target-interrupts.md`: interrupts and the tick (`timebase.*`, `midi_input.c`, `atmega328p_timer2_regs.h`, `init.c`).
- `target-uart.md`: loggers, MIDI out and in and the UART (`*logger*`, `*midi_output*`, `*midi_input*`, `atmega328p_usart*_regs.h`).
- `target-pins.md`: pins (`register_init.*`, `gate_output.*`, `clock_output.*`).
