# nano_sequencer

A bare-metal firmware project for the ATmega328P (Arduino Nano), written from scratch without the Arduino core and without any avr-libc headers or library functions. All peripheral access goes through registers declared in this repo and placed at their datasheet addresses by the linker, and nothing from the standard library is used: the serial logger does its own number-to-text conversion. The only code not written here is what the toolchain adds at link time: the chip's startup object (vector table and stack setup, which ships with avr-libc) and a few `libgcc` helpers.

## Status

Work in progress. The firmware initializes the ADC, USART, I/O direction registers and a 1 kHz timer tick, then steps through a 16-step pattern at a tempo of 30 to 285 BPM set by a pot. The main loop never blocks: once a millisecond it runs one tick of the sequencer, which reads the 16 step note values from a daisy-chained 74HC165 shift register bank and the tempo pot (every 8 ms) and, when the next step is due, plays it (for now that means logging the step and its pitch, or that it is a rest, over serial, in the debug build only). A step's four switches are its note value: 0 is a rest, and 1 to 15 are notes of a scale. The core can also play any range of the steps forward, in reverse or as a pendulum, looping or once, with a reset, and in five scales from any root; the board has no controls for those yet, so the firmware plays all 16 steps forward in the chromatic scale. The output stage that would make a step audible (gates, triggers, or CV out) is not yet implemented.

## Why bare-metal?

No Arduino `Wiring`/HAL layer and no avr-libc headers or functions in the source — registers are declared here and given their ATmega328P datasheet addresses at link time, and the compiler is involved in only two places: the `io_low` attribute on the low I/O registers and the `signal` attribute on the one interrupt handler. The fixed-width types come from the compiler's own `<stdint.h>` (the build uses `-ffreestanding`), not from avr-libc. This keeps the binary small and the behavior fully explicit at the register level.

## Project layout

The code follows a ports-and-adapters layout: the sequencer logic is a pure core with no hardware access, and everything that touches the MCU sits behind small port headers. The rules are in [CLAUDE.md](CLAUDE.md), with each folder's own rules in a `CLAUDE.md` beside its source, and the details in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

```
core/                         # Pure sequencer logic; also compiles on a PC
├── seq.c / seq.h                     # seq_tick(): raw inputs and elapsed ticks in, step + pitch out
├── clock.c / clock.h                 # clock_advance(): elapsed ticks to pulses at a tempo, with no drift
├── engine.h                          # The engine interface: what every engine is given and hands back
├── engine_plain.c / engine_plain.h   # The plain engine: each step's switches are its note value
├── address.c / address.h             # address_next(): direction, first and last step, one-shot, reset
├── note_map.c / note_map.h           # note_map_semitone(): note value to pitch through a scale and root
└── panel.c / panel.h                 # panel_step_value(): one step's four switches from the raw bytes
ports/                        # What the app needs from the outside world (headers only)
├── port_status.h                     # Shared status/error codes
├── platform_port.h                   # One-time platform bring-up
├── step_input_port.h                 # Raw step-note bits
├── tempo_input_port.h                # Raw tempo control position
├── timebase_port.h                   # Elapsed time in 1 ms ticks
└── log_port.h                        # Diagnostic output (queued; never blocks the loop)
adapters/
├── target/                   # The ports implemented for the ATmega328P
│   ├── init.c                        # Platform bring-up (logger + registers)
│   ├── register_init.c / .h          # I/O direction + ADC setup
│   ├── analog_reader.c               # Tempo input: ADC channel read
│   ├── shift_reg_reader.c            # Step input: 74HC165 shift register chain read
│   ├── timebase.c / .h               # Timebase: Timer/Counter2 at 1 kHz, the only interrupt
│   ├── logger.h                      # Logger bring-up and log levels, shared by the two loggers
│   ├── serial_logger.c               # Log, debug build: USART setup + queued text logging with log levels
│   ├── null_logger.c                 # Log, release build: discards every message
│   ├── bits.h                        # BIT_0..BIT_5 mask constants
│   ├── atmega328p_regs.h             # Register declarations and bit positions
│   ├── atmega328p_usart_regs.h       # The same for USART0, used only by the serial logger
│   ├── atmega328p_timer2_regs.h      # The same for Timer/Counter2, and its interrupt vector
│   └── atmega328p_regs.ld            # Register addresses, applied by the linker
└── host/                     # The ports faked for PC tests
    └── host_ports.c / .h             # Scripted inputs, recorded outputs
app/
├── app.c / app.h                     # Wires ports to the core: apply outputs, gather inputs, tick
└── main.c                            # Init, then the forever loop
tests/                        # Host tests (make test), including fake MCU registers
tools/misra/                  # cppcheck MISRA addon config and helper scripts
tools/coverage/               # Coverage report script (make coverage)
tools/check/                  # Runs every check and summarizes (make check)
tools/sim/                    # Emulated board (make sim)
├── lib/machine.js                    # The ATmega328P: avr8js CPU and peripherals
├── parts/hc165.js                    # 74HC165 chain model
├── boards/nano_sequencer.js          # What is wired to which pin
└── scenarios/*.test.js               # What each firmware image must do on that board
```

## Hardware target

- MCU: ATmega328P (as used on the Arduino Nano)
- Clock: 16 MHz
- USART: 115200 baud, 8N1, transmit only in practice (debug build; unused in the release build)
- Timer/Counter2: compare match interrupt at 1 kHz, the sequencer's tick (drives no pin)
- Analog inputs: ADC channel 6 (tempo control, 30 to 285 BPM)
- Step notes: 16 steps × 4 bits, read from a chain of 8 daisy-chained 74HC165 shift registers via `PORTD2` (SH/LD), `PORTD3` (CLK), and `PORTD4` (SER data-in) — each 74HC165's Clock Inhibit/CE pin must be tied to GND in hardware
- Digital I/O configured in `register_init.c`: `PORTB5`, `PORTD2`, `PORTD3` as outputs, `PORTC0`, `PORTD4` as inputs, `PORTC1` as output

## Dependencies

Building and flashing the firmware needs nothing beyond one AVR toolchain folder on your `PATH`. No Arduino IDE, no VS Code extension, and no libraries. The tests and the static analysis need three more tools, listed further down.

| Tool | Used for | Needed to |
|---|---|---|
| `avr-gcc` | Compiling and linking (also supplies the chip's startup code and `libgcc`) | Build |
| `avr-objcopy` | Converting the linked `.elf` to the `.hex` upload format | Build |
| `avr-size` | The flash/RAM usage report | Build |
| `make` | Running the `Makefile` | Build |
| `avrdude` | Uploading the `.hex` to the board over USB serial | Flash only |

Installing them:

- **Windows:** prebuilt AVR toolchain bundles typically ship all five in a single `bin` folder, so adding that folder to `PATH` is the whole setup.
- **macOS** (Homebrew; `make` comes with the Xcode command line tools):
  ```sh
  brew tap osx-cross/avr
  brew install avr-gcc avrdude
  ```
- **Linux** (Debian/Ubuntu; `avr-libc` is needed for the chip's startup object at link time):
  ```sh
  sudo apt install gcc-avr binutils-avr avr-libc avrdude make
  ```

Developed on Windows with avr-gcc 12.1.0, GNU Make 4.2.1, and avrdude 7.0. The `Makefile` detects the platform and is written to work on macOS and Linux as well, but it has not yet been run on either.

For development, four optional checks need extra tools on `PATH`:

| Tool | Used for | Needed to |
|---|---|---|
| `node` and `npm` (Node.js 22 or later) | Running the firmware images on an emulated board. The first run downloads the [avr8js](https://github.com/wokwi/avr8js) emulator library into `tools/sim/node_modules` | `make sim` |
| A C compiler for your PC (`gcc` on Windows, `cc` elsewhere) | Building the code and its tests for the PC | `make test`, `make coverage` |
| `gcov` (comes with gcc) | Counting which lines and branches the tests run | `make coverage` |
| `cppcheck` | Static analysis with its MISRA addon | `make misra` |
| `python` | Running cppcheck's MISRA addon and the coverage report | `make misra`, `make coverage` |

On Windows with Chocolatey, `choco install mingw cppcheck` covers the first two. `make misra` also needs the MISRA rule headlines file, which is copyrighted and not in this repo; fetch it once with `tools/misra/fetch_misra_headlines.sh`. Developed with MinGW gcc 16.1.0 and Cppcheck 2.19.0.

On the hardware side: an Arduino Nano (ATmega328P, 16 MHz) with its stock serial bootloader, and a USB cable. The upload speed in the `Makefile` (57600 baud) is for the old Nano bootloader; newer boards use 115200.

## Building and flashing

Build and flash with the included `Makefile`:

```sh
make            # Compile + link + convert to build/debug/output.hex, then print flash/RAM usage
make size       # Print flash/RAM usage (builds first if needed)
make flash      # Flash build/debug/output.hex
make test       # Build the firmware source for your PC and run the unit tests
make coverage   # Run the tests instrumented; fails unless line and branch coverage is 100%
make misra      # Run cppcheck with the MISRA addon
make sim        # Run both firmware images on an emulated board and check what they do
make check      # Both builds and all four checks above; one line each unless one fails
make clean      # Remove the build/ directory
```

There are two build configurations, chosen with `CONFIG=`. They differ only in which logger is linked; every file is compiled the same way for both.

| | Logging | Output | Flash | Static RAM |
|---|---|---|---:|---:|
| `make` (same as `CONFIG=debug`) | Text over USART0 at 115200 baud | `build/debug/output.hex` | 2534 bytes | 382 bytes |
| `make CONFIG=release` | None; the USART is never switched on | `build/release/output.hex` | 1800 bytes | 94 bytes |

`CONFIG` applies to `make`, `make size` and `make flash` (for example `make flash CONFIG=release`). The tests, the coverage check and the static analysis always cover both loggers.

The firmware is compiled with `-std=c99 -Wall -Wextra -Wconversion -Wshadow -Werror`, so any warning fails the build. `make test` never touches the board: it links the real core and app loop against fake ports and checks what they do, and it compiles each MCU adapter against fake registers to check what it does with them (the exact serial text, the shift register pulse sequence, the ADC timeout, the timer settings, and so on). Time is scripted in these tests, so they check on which millisecond tick each thing happens.

`make coverage` runs the same tests built with gcc's coverage instrumentation and prints a table per source file. Every file in `core/`, `app/` and `adapters/target/` must have all of its lines run and all of its branches taken, or the command fails. That is measured on the PC build: it shows the logic is exercised, not that register addresses or pulse timing are right on the chip.

`make sim` builds both configurations and runs the two `output.hex` files, unmodified, on an emulated ATmega328P with the 74HC165 chain, the tempo pot and a serial capture modelled around it. The scenarios check the log text against the switches set on the emulated panel, the load and clock pulses of each panel read and how often it is read, the timer settings, and the length of a step at a given tempo, all counted in CPU cycles. It checks the real machine code, which the host tests cannot, but against models of the chip and the parts: the board is still the final check.

`make check` runs both builds, the tests, coverage, the MISRA analysis and the emulated board in one go. A stage that passes prints one line (with the flash and RAM figures, the coverage total or the scenario count); a stage that fails prints its full output. Every stage runs even if an earlier one failed, and the command fails if any did. It needs all the tools in the table above.

`make flash` picks the serial port per platform: `COM3` on Windows, the first `/dev/cu.usbserial*`, `/dev/cu.wchusbserial*` or `/dev/cu.usbmodem*` device on macOS, and the first `/dev/ttyUSB*` or `/dev/ttyACM*` device on Linux. Override it if your Nano enumerates differently, e.g. `make flash PORT=COM4` or `make flash PORT=/dev/ttyUSB1`. On Linux your user needs access to the port (usually membership of the `dialout` group).

## Coding standard

If you fork this or send changes, write to the same standard. The firmware source (`core/`, `ports/`, `adapters/` and `app/`) follows BARR-C:2018 (the Barr Group Embedded C Coding Standard) for style and naming, plus these MISRA-style safety rules:

- Use C99 and fixed-width types from `<stdint.h>` for anything with a meaningful size.
- Don't allocate dynamically (no malloc/free) and don't use recursion. All memory is static or on the stack, with bounded sizes.
- Every `switch` has a `default`, and there's no fallthrough without a comment.
- Every `if`/`else`/`for`/`while` body uses braces.
- Each function has a single, clear purpose. Keep functions short and nesting shallow.
- Don't use implicit conversions that lose data or change signedness. Cast explicitly and make sure the cast is justified.
- Check the return value of every function that can fail, or explicitly cast it to `(void)`.
- Use no magic numbers. Use named constants or enums.
- Make anything not used outside its file `static`. Minimize globals, and mark globals shared with an ISR `volatile` and access them atomically or with interrupts masked.
- Don't use the preprocessor for logic when a `static inline` function or an enum works.
- Document every port function in its header: purpose, units, valid ranges, and whether it's ISR-safe.

Line length: aim for 80 characters and never exceed 100. Some existing lines are over; new and changed lines should not be.

The checks that back this up are the warning flags the firmware is built with, `make test`, `make coverage`, `make misra` and `make sim`, described above. MISRA C is a secondary, automated check through cppcheck, which implements only part of it; how findings and deviations are handled is set out in [tools/misra/CLAUDE.md](tools/misra/CLAUDE.md). Test code in `tests/` and the emulation tooling in `tools/sim/` are outside the standard.

## License

Apache License 2.0 — see [LICENSE](LICENSE).
