# adapters/target/ rules

The ports implemented for the ATmega328P. This is the only folder that touches hardware.

## Registers

- **Registers live in two files.** `atmega328p_regs.h` declares each register as `extern volatile`; `atmega328p_regs.ld` gives it its address and is passed to the linker as an extra input. A new register needs both. Use the plain datasheet data address (for example `0x2B`). A register missing from the `.ld` file fails at link time; a wrong address links silently, so check it against the datasheet.
- Registers at 0x20 to 0x3F need the `REG_IO_LOW` attribute or the compiler stops using `sbi`/`cbi`, which changes the shift register load pulse.
- Putting the address inside the attribute does not work on avr-gcc 12.1.0 (`io_low(0x2B)` fails with "IO definition needs an address"), which is why the linker file exists.
- **A macro used by only one build configuration is a rule 2.5 finding in the other.** Definitions that only one configuration's files use go in a header only those files include. That is why the USART0 registers and bit positions are in `atmega328p_usart_regs.h` (same `.ld` file), included only by `serial_logger.c` and `midi_output.c`, which are never linked together. Each of the two must use every macro in it, or the other's run reports rule 2.5: that is why the MIDI adapter enables the receiver (`RXEN0`) it does not read. A bit only one of them needs means splitting the header.
- Group a new peripheral's registers in a header of its own on the same pattern (`atmega328p_timer2_regs.h`, `atmega328p_spi_regs.h` and the rest), with only the bit positions the code uses, and check each position against the toolchain's `avr/iom328p.h`. An unused bit position is a rule 2.5 finding, and so is an unused mask in `bits.h`.
- A new register also goes into `tests/fake_atmega328p_regs.h` and its `fake_regs_reset()`.

## Drivers and devices

- Two levels. A **driver** runs one MCU peripheral and implements no port (`spi.c`); its header is private to this folder. A **device adapter** implements a port for one external part, using a driver (`shift_reg_reader.c` implements `step_input_port.h` on the SPI driver, and `cv_output.c` implements `cv_output_port.h` on it for the MCP4822).
- A driver owns its peripheral's registers and pins: only `spi.c` includes `atmega328p_spi_regs.h` or sets the direction of SCK, MOSI and SS. A device adapter owns the part's own lines (the 74HC165 load line, a chip select). SS is both: `spi.c` makes it a high output, which master mode needs, and `cv_output.c` then drives it as the DAC's chip select.
- A chip select is released on every path out of a write, failures included. A part that ignores a short frame (the MCP4822) is then left as it was.
- `shift_reg_reader.c` implements two ports, the step input and the step lights, because both chains hang on PB1: it is the 74HC165s' load line and the 74HC595s' latch clock. Nothing else may move PB1. The lights' two bytes are sent with the line idle, immediately before the load pulse whose rising edge latches them; keep anything else off the bus between the two, or it is what gets latched. `led_output_write()` only stores the value.
- A part with no chip select (the 74HC165 chain) is clocked by every transfer on the bus. That is harmless only because it is loaded afresh before each read; keep it so.
- A driver call that waits is bounded by a poll budget and returns `ERR_TIMEOUT`, like the ADC read.
- The SPI is master, mode 0, MSB first, 1 MHz. Every part on the bus must work in that mode, or the mode becomes a parameter of the transfer. SS (PB2) stays an output: as a low input it cancels master mode.
- A pin that idles high is set high before it is made an output.
- A driver's init goes in `platform_init()` after `register_init()` and before `timebase_init()`.

## EEPROM and reset cause

- `eeprom.c` implements `nv_port.h` and owns the EEPROM registers; nothing else includes `atmega328p_eeprom_regs.h`. It is one file with no driver under it, like `timebase.c`, because the peripheral has one user: the crash log goes through `nv_port` from the app like the calibration record, so it is not a second one. Split the file if something in this folder needs the EEPROM.
- Nothing waits for the EEPROM. `nv_write()` only copies the bytes; `nv_poll()` starts at most one byte per call, and only when the hardware's write bit is clear. A byte that already holds the value is skipped.
- Starting a byte is a timed sequence: EEMPE, then EEPE within four clock cycles. Keep the two writes adjacent, with interrupts masked around just them and the status register restored as found, and check with `avr-objdump -d` that they compile to an `out` followed directly by an `sbi`. `EECR` needs `REG_IO_LOW` for that.
- A read while a write is in hand is refused, not waited for: it would move the address register.
- The calibration record is at address 0 and the crash log at 16 (the app's choices, not this file's). Another user of the EEPROM takes other addresses.
- `reset_cause.c` passes on the low four bits of `MCUSR` as they are, because the port gives the causes in that order, and clears the register. Read it once, at start-up. What it holds on the board depends on the bootloader (`docs/ROADMAP.md` section 8).

## Breadcrumb

- `breadcrumb.c` implements `breadcrumb_port.h` with one byte marked `RAM_NOINIT` (in `atmega328p_regs.h`), which puts it in the `.noinit` section: the startup code neither clears nor sets it, so it keeps its value through a reset with the power on.
- Do not give it an initialiser; that would move it to `.data` and the startup code would set it on every start.
- Check with `avr-objdump -h` that the image has a `.noinit` section of one byte, and with `avr-nm` that the byte sits at `__bss_end`, past what the startup code clears.
- It is not `volatile` and no interrupt touches it. After power-on it holds anything; what it means is the app's business.
- The fake register header defines `RAM_NOINIT` as nothing. That the byte survives a reset is shown on the emulated board (`watchdog.test.js`), not by the host test.

## Watchdog

- `watchdog.c` implements `watchdog_port.h` and owns `WDTCSR`; nothing else includes `atmega328p_wdt_regs.h`.
- Setting the timeout is two writes that must be within four clock cycles of each other. Keep them adjacent and check with `avr-objdump -d` that they are `ldi`, `sts`, `ldi`, `sts` with nothing between. They are not wrapped in an interrupt mask because `watchdog_init()` runs before `timebase_init()`; if it ever runs with interrupts on, mask them as `eeprom.c` does.
- `watchdog_init()` goes after everything in `platform_init()` that can fail, so a failed bring-up halts and is not reset for ever, and it must stay well within 16 ms of reset: after a watchdog reset the hardware has the watchdog running at that timeout already.
- It leaves `MCUSR` alone. The reset cause is read, and the flags cleared, by `reset_cause.c` afterwards.
- `wdr` comes from `__builtin_avr_wdr`, declared in the register header so the call has a prototype, inside a `static inline` function the fake header replaces. cppcheck accepts the declaration.
- The stock old bootloader does not turn the watchdog off, so on that bootloader a watchdog reset loops until the power is cycled (`docs/ARCHITECTURE.md` open item 15).

## Edge inputs

- `clock_input.c` (PB0, Timer/Counter1's input capture flag) and `reset_input.c` (PD2, the flag of external interrupt 0) each poll a flag the MCU latches on an edge. Each owns its peripheral's registers, like `eeprom.c`, and has an init function that `platform_init()` calls after `register_init()` and before `timebase_init()`.
- The interrupts those flags belong to stay off, and each init writes the mask register to make sure. Neither vector has a handler: an enabled one would land in `__bad_interrupt` and restart the firmware.
- A flag is cleared by writing a one to it. Write the mask with a plain assignment (`TIFR1 = CAPTURE_FLAG;`), never `|=`: that reads the register and writes back a one for every flag that happens to be set, clearing them all.
- Clear the flag as the last step of init. Choosing the edge can set it.
- The edge is the falling one at the pin, because the input stage between jack and pin is taken to invert. If the stage built does not, change the edge in the two init functions and nowhere else.
- That a flag latches with its interrupt off has not been tried on the chip, and the emulator's model of it was written here (`docs/ROADMAP.md` section 8).
- `run_input.c` (PD7) reads a pin that is low while the contact is closed. It debounces nothing; the core does.

## The ADC

- `analog_reader.c` owns the ADC and implements two ports on it: the pot (`pot_input_port.h`, channel 6) and the modulation input (`mod_input_port.h`, channel 7). Each read sets the channel itself, so neither depends on what the other left.
- A read waits for its conversion, about 0.1 ms, bounded by a poll budget. Both are read in the 8 ms scan, one after the other, so a scan tick is about 0.2 ms longer than the rest; the outputs are applied before it, so no edge moves. Something read on every tick would add its conversion to every tick.
- The modulation input's stage has to put a signal at its centre, or no signal at all, at half the reference: a reading of 512. The firmware takes anything within 16 of that as nothing. The stage is not designed (`docs/ROADMAP.md` section 8).

## The level light

- `level_output.c` implements `level_output_port.h` and owns Timer/Counter0; nothing else includes `atmega328p_timer0_regs.h`. The timer runs fast PWM, 8 bits, system clock / 64 (977 Hz), with no interrupt, and its compare output A is PD6.
- Fast PWM cannot give a flat low: a compare value of 0 is still one count high in 256. A level of 0 therefore takes the timer off the pin (the compare output bits cleared), and the pin falls back to the port's level, which `register_init.c` left low. Nothing may write PD6 in `PORTD`, or a dark light is not dark.
- The compare register is buffered in this mode and taken up as the count returns to 0, so writing it on every tick never cuts a pulse.
- The bit positions and the three addresses were checked against the toolchain's `avr/iom328p.h`, and the emulator gives the duty asked for. The pin has not been tried on the board.

## Compiler

- **Compiler builtins need a visible prototype** or cppcheck reports rule 17.3. One is in use, `__builtin_avr_wdr` (see Watchdog); avr-gcc accepts the redeclaration and still inlines it.
- The `io_low`, `signal` and `section` attributes are compiler extensions that cppcheck does not flag.

## Interrupts

- An ISR does the minimum (a counter, a flag, a ring buffer) and never calls into the core or a port.
- Each variable shared with an ISR is a file-static `volatile` in exactly one `.c` file and is reached from outside only through a port function. Keep it to one byte so a read is a single instruction; anything wider needs interrupts masked around the access.
- The handler's name and attribute come from macros in the peripheral's register header (`TIMER2_COMPA_ISR`, `TIMER2_COMPA_ISR_ATTR`). The fake register header redefines them, so the host test calls the handler as an ordinary function.
- A handler must have external linkage for the vector table to find it. cppcheck does not see the vector table (it is machine code in the startup object), so it reports rule 8.7 (advisory). That is a false positive; suppress it inline with the reason, as `timebase.c` does.
- Check a new vector's number against the datasheet and confirm with `avr-objdump -d` that the vector table entry jumps to the handler. A wrong number links silently.
- `timebase_init()` is the last step of `platform_init()` because it sets the global interrupt enable bit. Anything that must be set up before interrupts run goes before it.

## Loggers, MIDI outputs and the UART

- There is one USART and two link-time pairs that could want it: the logger (`serial_logger.c` or `null_logger.c`) and the MIDI output (`midi_output.c` or `null_midi_output.c`). The Makefile links one of each, and at most one file across both pairs may touch the USART: the serial logger in debug, the MIDI output in midi, neither in release. A new configuration is a new choice from the pairs, never a macro.
- `serial_logger.c` (debug) and `null_logger.c` (release and midi) both implement `log_port.h` and `logger.h`. A change to either interface changes both files. The same goes for `midi_output.c` (midi) and `null_midi_output.c` (debug and release), which implement `midi_output_port.h` and `midi_output.h`.
- `null_midi_output.c` never touches a register and gives a room of 255. That is what keeps the release build off PD0 and PD1.
- MIDI's 31250 baud is exact in double-speed mode at 16 MHz, a divisor of 63. A byte is 320 µs, so about 3 leave in a tick against up to 8 queued in one; the 32-byte queue is four worst-case ticks. `midi_output_write()` only queues and `midi_output_poll()` sends one byte per call when the transmitter is ready.
- A port function that calls a sibling port function keeps a rule 8.7 finding alive for the one called. Both use a static helper instead (`queue_room()` behind `midi_output_room()` and `midi_output_write()`).
- `midi_output_init()` returns nothing, since it cannot fail, and goes with the driver inits in `platform_init()`, before the watchdog.
- PD1 floats from reset until the transmitter is enabled. That matters for the MIDI out circuit and has not been tried (`docs/ARCHITECTURE.md` open item 16).
- The serial logger never waits for the UART. A message is written into a 128-byte queue and `log_poll()` sends one byte per call when the transmitter is ready. A message that does not fit is dropped whole, so write every message between `begin_message()` and `end_message()`. Only `log_flush()` blocks.
- The serial rate is 115200 baud in double-speed mode, which is 2.1 % fast at 16 MHz. 250000, 500000 and 1000000 are exact.
- The release build never enables the USART. PD0 and PD1 are not configured by `register_init.c`; leave them alone so the release build does not drive them against the board's USB serial chip. In the midi build PD1 also reaches that chip, so the PC's port shows the MIDI bytes as noise.

## Pins

- The 74HC165 chain is ten chips: eight for the steps, one for the mode switches and, farthest from the MCU, one whose low three inputs are the selector. Another chip goes on the far end, so the bytes already read keep their places; it costs a constant in `shift_reg_reader.c`, the port header's range and the two fakes.
- In use: PB1 (74HC165 load), PB5 (SCK, the clock of the chain and the DAC), PB4 (MISO, the chain's data, through 1 kΩ), PB3 (MOSI, the data of the DAC and of the step lights' 74HC595s), PB2 (SS, the DAC's chip select, idle high), PD4 (gate output), PD5 (clock output), ADC channel 6 (the pot), ADC channel 7 (the modulation input), PB0 (clock input), PD2 (reset input), PD7 (run button), and PD1 as the USART's transmit pin: the log in the debug build, MIDI out in the midi build (PD0 is then its receive input, which nothing reads). Timer/Counter2 is the 1 kHz tick and drives no pin, and the EEPROM has none either. Timer/Counter1 runs only for its capture flag, and Timer/Counter0 only for the level light.
- The MCP4822 is used on channel A at a gain of 2 from its internal reference (1 mV a step), with LDAC tied low. Its frame layout and pinout have been checked against the datasheet (`docs/ROADMAP.md` section 8).
- PD3 is free, and left an input with its pull-up on so that it does not float. PD6 is the level light, driven by Timer/Counter0. PC0 and PC1 are given a direction but are unused and free.
- An input pin's direction and pull-up are set in `register_init.c` too. All three inputs use the MCU's own pull-ups.
- An output pin's direction and idle level are set in `register_init.c`, level first. The adapter that writes it (`gate_output.c`, `clock_output.c`) only changes its level, one pin per statement so that it compiles to a single `sbi` or `cbi` and cannot disturb the rest of the port. Check that with `avr-objdump -d` for a new one.
- The gate and clock outputs are active high. If a buffer stage inverts them, invert in those two files and nowhere else.
- PB3 to PB5 are also the ISP programmer's pins; the 1 kΩ on MISO is what lets a programmer override the chain.

## Testing and analysis

- Every `.c` file here has a host test that `#include`s it against fake registers, and must stay at 100% of lines and branches. `tests/CLAUDE.md` has the recipe.
- This folder gets more MISRA leeway than `core/`, because hardware access legitimately needs things like `volatile`. It has one inline suppression, a false positive (rule 8.7 on the tick interrupt handler), and no true deviations; `tools/misra/CLAUDE.md` says how to add one if it is truly needed.
- When a part or pin changes here, update its model or wiring in `tools/sim/` in the same change.
