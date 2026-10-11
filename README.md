# nano_sequencer

A bare-metal firmware project for the ATmega328P (Arduino Nano), written from scratch without the Arduino core and without any avr-libc headers or library functions. All peripheral access goes through registers declared in this repo and placed at their datasheet addresses by the linker, and nothing from the standard library is used: the serial logger does its own number-to-text conversion. The only code not written here is what the toolchain adds at link time: the chip's startup object (vector table and stack setup, which ships with avr-libc) and a few `libgcc` helpers.

## Status

Work in progress. The firmware initializes the ADC, USART, I/O direction registers, the SPI bus and a 1 kHz timer tick, then steps through a 16-step pattern at a tempo of 30 to 285 BPM set by a pot. The main loop never blocks: once a millisecond it runs one tick of the sequencer, which reads the 16 step note values from a daisy-chained 74HC165 shift register bank over the SPI bus and the tempo pot (every 8 ms) and, when the next step is due, plays it: if the step has a note, a pitch control voltage is set through an MCP4822 DAC (1 V to the octave) and a gate output goes high for half the step; a clock output pulses once per step, and the debug build also logs the step and its pitch, or that it is a rest, over serial. A step's four switches are its note value: 0 is a rest, and 1 to 15 are notes of a scale. The core can also play any range of the steps forward, in reverse or as a pendulum, looping or once, and in five scales from any root; the board has no controls for those yet, so the firmware plays all 16 steps forward in the chromatic scale. A run button stops the pattern and starts it again from its first step, and a reset input sends it back to that step at the next step boundary. A sync switch chooses the clock: off, the tempo pot sets the tempo; on, a step begins on each edge at a clock input, and the pot becomes a ratio to that clock, from one step every four edges to four steps an edge. None of those four inputs has been wired on the real board yet. The gate and clock outputs are bare logic pins and the pitch CV is the DAC's own output, with no buffer stage on any of them yet. The pitch goes through a calibration table with an entry per octave, which is the ideal one until a measured one is stored: powering up with a reserved pattern on the last four steps gives a calibration mode, in which the first step's switches choose an octave, the tempo pot trims its voltage against a meter, and a switch stores it in the MCU's EEPROM. At start-up the firmware loads that table if its version and checksum are good, and the debug build logs why the processor reset. The DAC has not been on the real board yet, so neither has calibration.

## Why bare-metal?

No Arduino `Wiring`/HAL layer and no avr-libc headers or functions in the source — registers are declared here and given their ATmega328P datasheet addresses at link time, and the compiler is involved in only two places: the `io_low` attribute on the low I/O registers and the `signal` attribute on the one interrupt handler. The fixed-width types come from the compiler's own `<stdint.h>` (the build uses `-ffreestanding`), not from avr-libc. This keeps the binary small and the behavior fully explicit at the register level.

## Project layout

The code follows a ports-and-adapters layout: the sequencer logic is a pure core with no hardware access, and everything that touches the MCU sits behind small port headers. The rules are in [CLAUDE.md](CLAUDE.md), with each folder's own rules in a `CLAUDE.md` beside its source, and the details in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

```
core/                         # Pure sequencer logic; also compiles on a PC
├── seq.c / seq.h                     # seq_tick(): raw inputs and elapsed ticks in, step, pitch, CV, gate + clock out
├── clock.c / clock.h                 # Elapsed ticks to pulses: at a tempo with no drift, or following external edges
├── transport.c / transport.h         # transport_tick(): stopped, or running from which clock
├── debounce.c / debounce.h           # debounce_update(): the settled level of a button or switch
├── engine.h                          # The engine interface: what every engine is given and hands back
├── engine_plain.c / engine_plain.h   # The plain engine: each step's switches are its note value
├── engine_stage.c / engine_stage.h   # The stage engine: eight stages with a length and a gate mode
├── engine_klee.c / engine_klee.h     # The Klee engine: bits round a register, the note their sum
├── engine_chance.c / engine_chance.h # The chance engine: steps skipped or shuffled at random
├── engine_cartesian.c / .h           # The Cartesian engine: a 4 by 4 grid and two counters
├── engine_euclid.c / engine_euclid.h # The Euclidean engine: beats spread evenly over a bar
├── prng.c / prng.h                   # The random numbers the engines draw from
├── address.c / address.h             # address_next(): direction, first and last step, one-shot, reset
├── note_map.c / note_map.h           # note_map_semitone(): note value to pitch through a scale and root
├── gate.c / gate.h                   # gate_advance(): how long a note is held, in clock pulses; ties
├── pitch_cal.c / pitch_cal.h         # pitch_cal_cv(): pitch to CV value through a per-octave table
├── cal.c / cal.h                     # cal_tick(): calibration mode, setting that table from the panel and pot
├── cal_record.c / cal_record.h       # The table as stored bytes, with a version and a checksum
└── panel.c / panel.h                 # panel_step_value(): one step's four switches from the raw bytes
ports/                        # What the app needs from the outside world (headers only)
├── port_status.h                     # Shared status/error codes
├── platform_port.h                   # One-time platform bring-up
├── step_input_port.h                 # Raw step-note bits
├── led_output_port.h                 # The sixteen step lights
├── tempo_input_port.h                # Raw tempo control position
├── timebase_port.h                   # Elapsed time in 1 ms ticks
├── cv_output_port.h                  # The pitch control voltage, in millivolts
├── gate_output_port.h                # The gate output's level
├── clock_output_port.h               # The clock output's level
├── clock_input_port.h                # Edges on the external clock input
├── reset_input_port.h                # Edges on the reset input
├── run_input_port.h                  # The run button, as read
├── sync_input_port.h                 # The sync switch, as read
├── nv_port.h                         # Storage that survives power-off (written a byte per loop pass)
├── reset_cause_port.h                # Why the processor last started
└── log_port.h                        # Diagnostic output (queued; never blocks the loop)
adapters/
├── target/                   # The ports implemented for the ATmega328P
│   ├── init.c                        # Platform bring-up (logger, registers, SPI, edge inputs, timebase)
│   ├── register_init.c / .h          # I/O direction + ADC setup
│   ├── analog_reader.c               # Tempo input: ADC channel read
│   ├── shift_reg_reader.c            # Step input and step lights: 74HC165 and 74HC595 chains, over SPI
│   ├── spi.c / spi.h                 # SPI driver: the MCU as bus master, used by the adapters above
│   ├── timebase.c / .h               # Timebase: Timer/Counter2 at 1 kHz, the only interrupt
│   ├── cv_output.c                   # Pitch CV output: MCP4822 DAC, over SPI
│   ├── gate_output.c                 # Gate output: PD4
│   ├── clock_output.c                # Clock output: PD5
│   ├── clock_input.c / .h            # Clock input: PB0, Timer/Counter1's capture flag, polled
│   ├── reset_input.c / .h            # Reset input: PD2, the INT0 flag, polled
│   ├── run_input.c                   # Run button: PD7
│   ├── sync_input.c                  # Sync switch: PD3
│   ├── eeprom.c                      # Non-volatile storage: the MCU's EEPROM, never waited for
│   ├── reset_cause.c                 # Reset cause: the MCU's reset flags, read and cleared
│   ├── logger.h                      # Logger bring-up and log levels, shared by the two loggers
│   ├── serial_logger.c               # Log, debug build: USART setup + queued text logging with log levels
│   ├── null_logger.c                 # Log, release build: discards every message
│   ├── bits.h                        # Single-bit mask constants
│   ├── atmega328p_regs.h             # Register declarations and bit positions
│   ├── atmega328p_usart_regs.h       # The same for USART0, used only by the serial logger
│   ├── atmega328p_timer1_regs.h      # The same for Timer/Counter1, used only by the clock input
│   ├── atmega328p_timer2_regs.h      # The same for Timer/Counter2, and its interrupt vector
│   ├── atmega328p_extint_regs.h      # The same for the external interrupts, used only by the reset input
│   ├── atmega328p_spi_regs.h         # The same for the SPI, used only by the SPI driver
│   ├── atmega328p_eeprom_regs.h      # The same for the EEPROM, used only by the EEPROM adapter
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
├── lib/pin_recorder.js               # Edges and pulses on one output pin
├── lib/edge_flag.js                  # A flag that latches an edge on an input pin, interrupt on or off
├── parts/hc165.js                    # 74HC165 chain model
├── parts/hc595.js                    # 74HC595 chain model
├── parts/mcp4822.js                  # MCP4822 DAC model
├── parts/clock_source.js             # Something that sends pulses: a clock, a reset
├── parts/switch.js                   # A switch or button, with bounce
├── boards/nano_sequencer.js          # What is wired to which pin
├── boards/cal_record.js              # The calibration record, worked out apart from the firmware
├── scenarios/*.test.js               # What each firmware image must do on that board
└── live/server.js, page.html         # The same board at real speed, in a browser (make live)
```

## Hardware target

The wiring is drawn pin by pin, with a diagram of each chip, in [docs/SCHEMATIC.html](docs/SCHEMATIC.html) (open it in a browser).

- MCU: ATmega328P (as used on the Arduino Nano)
- Clock: 16 MHz
- USART: 115200 baud, 8N1, transmit only in practice (debug build; unused in the release build)
- Timer/Counter2: compare match interrupt at 1 kHz, the sequencer's tick (drives no pin)
- Timer/Counter1: only its input capture flag is used, to latch an edge on the clock input; no interrupt
- Analog inputs: ADC channel 6 (tempo control, 30 to 285 BPM)
- EEPROM: 12 of its 1024 bytes, from address 0, hold the pitch calibration record. A chip erase by an ISP programmer wipes it unless the EESAVE fuse is set; an upload through the bootloader does not
- SPI: master, mode 0, MSB first, 1 MHz, shared by the shift registers and the DAC
- Pitch CV: channel A of an MCP4822 12-bit DAC (internal 2.048 V reference, gain 2: 1 mV a step, 0 to 4.095 V). `PORTB2` (D10, SS) is its chip select, `PORTB5` (D13, SCK) its clock and `PORTB3` (D11, MOSI) its data; its LDAC pin is tied to GND. 1 V to the octave from 0 V at the lowest pitch, written every millisecond, through the calibration table. No buffer or protection yet
- Step notes and mode switches: 16 steps × 4 bits and 8 mode switches, read from a chain of 9 daisy-chained 74HC165 shift registers on the SPI bus (the ninth, farthest from the MCU, holds the mode switches, whose low three choose the engine): `PORTB1` (D9) is SH/LD, `PORTB5` (D13, SCK) is CLK, and `PORTB4` (D12, MISO) takes QH of the nearest chip through 1 kΩ — each 74HC165's Clock Inhibit/CE pin must be tied to GND in hardware
- Gate output: `PORTD4` (D4), high while a note is held, which is half a step. Clock output: `PORTD5` (D5), high for the first half of every step. Both are 0 V / 5 V logic straight from the MCU, with no buffer or protection yet
- Clock input: `PORTB0` (D8, ICP1). Reset input: `PORTD2` (D2, INT0). Each jack is meant to reach its pin through a transistor that pulls the pin low while the jack is high, so the firmware looks for a falling edge at the pin; the flag that latches it is polled once a millisecond and no interrupt is used. That input stage is not built yet
- Sync switch: `PORTD3` (D3) to ground, on when closed. Run button: `PORTD7` (D7) to ground. All four of these inputs use the MCU's own pull-ups
- Other digital I/O configured in `register_init.c`: `PORTC0` as input, `PORTC1` as output; neither is used yet. `PORTD6` is free

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
| `node` and `npm` (Node.js 22 or later) | Running the firmware images on an emulated board. The first run downloads the [avr8js](https://github.com/wokwi/avr8js) emulator library into `tools/sim/node_modules` | `make sim`, `make live` |
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
make live       # Run one image on that board at real speed and watch it in a browser
make check      # Both builds and all four checks above; one line each unless one fails
make clean      # Remove the build/ directory
```

There are two build configurations, chosen with `CONFIG=`. They differ only in which logger is linked; every file is compiled the same way for both.

| | Logging | Output | Flash | Static RAM |
|---|---|---|---:|---:|
| `make` (same as `CONFIG=debug`) | Text over USART0 at 115200 baud | `build/debug/output.hex` | 8132 bytes | 653 bytes |
| `make CONFIG=release` | None; the USART is never switched on | `build/release/output.hex` | 7056 bytes | 199 bytes |

`CONFIG` applies to `make`, `make size` and `make flash` (for example `make flash CONFIG=release`). The tests, the coverage check and the static analysis always cover both loggers.

The firmware is compiled with `-std=c99 -Wall -Wextra -Wconversion -Wshadow -Werror`, so any warning fails the build. `make test` never touches the board: it links the real core and app loop against fake ports and checks what they do, and it compiles each MCU adapter against fake registers to check what it does with them (the exact serial text, the shift register load pulse and transfers, the SPI settings, the ADC timeout, the timer settings, the frame sent to the DAC, the gate and clock pins, the EEPROM's timed write sequence, the two edge flags being set up, read and cleared, and so on). Time is scripted in these tests, so they check on which millisecond tick each thing happens.

`make coverage` runs the same tests built with gcc's coverage instrumentation and prints a table per source file. Every file in `core/`, `app/` and `adapters/target/` must have all of its lines run and all of its branches taken, or the command fails. That is measured on the PC build: it shows the logic is exercised, not that register addresses or pulse timing are right on the chip.

`make sim` builds both configurations and runs the two `output.hex` files, unmodified, on an emulated ATmega328P with the 74HC165 chain, the MCP4822 DAC, the tempo pot, a clock source and a reset source on the two input jacks, the sync switch, the run button (with bounce), recorders on the gate and clock pins and a serial capture modelled around it. The scenarios check the log text against the switches set on the emulated panel, the load pulse and SPI transfers of each panel read and how often it is read, the SPI and timer settings, the length of a step at a given tempo, the width and timing of every gate and clock pulse, that the DAC holds each note's voltage before its gate rises, that a calibration stored in the emulated EEPROM in calibration mode is played from after a power cycle, and that the pattern follows the clock input, the pot's ratio, the reset input, the run button and the sync switch, all counted in CPU cycles. It checks the real machine code, which the host tests cannot, but against models of the chip and the parts: the board is still the final check. That holds most of all for the two edge inputs, where the emulator had no model of a flag latching with its interrupt off and this project wrote one from the datasheet.

`make live` builds both configurations, runs one of them on the same emulated board at the pace of a real chip, and prints an address (`http://localhost:8165/`; `LIVE_PORT=` changes the port) to open in a browser. The page shows the panel's 64 switches and the tempo pot, which can be worked while it plays, the step being played, the gate and clock pins, the pitch CV as a figure and a trace, and the serial log; it can play the pitch CV and gate as a tone, switch between the two images, and power-cycle the board with its EEPROM kept or erased. A button enters calibration mode (it sets the entry pattern and power-cycles), and the page then says what the controls do in the mode and how to leave it. It also has the run button, the sync switch, a clock source for the clock input whose rate can be set, single clock and reset pulses, and a line saying whether the pattern is stopped or which clock it is running from and what the pot means just then. It is for looking, not checking: nothing is asserted, it is paced by the PC's clock, and it is not part of `make check`. It serves this PC only, and Ctrl+C stops it.

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
