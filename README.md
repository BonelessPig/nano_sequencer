# nano_sequencer

A bare-metal step sequencer firmware for the ATmega328P (Arduino Nano), written from scratch without the Arduino core and without any avr-libc headers or library functions.
Registers are declared in this repo and placed at their datasheet addresses by the linker.
The only code not written here is what the toolchain adds at link time: the chip's startup object and a few `libgcc` helpers.

## Status

Work in progress.
The firmware plays a 16-step pattern at 30 to 285 BPM, and its main loop never blocks: a 1 kHz timer tick runs one pass of the sequencer a millisecond.

- **Panel.** Each step has four switches, its note value: 0 is a rest and 1 to 15 are notes of a scale.
  Eight mode switches choose one of six engines (plain, stage, Klee, chance, Cartesian, Euclidean) and set its options.
- **Outputs.** A pitch CV at 1 V to the octave from an MCP4822 DAC, a gate, a clock pulse per step, sixteen step lights, and a level light.
- **Clock and transport.** A run button stops and restarts the pattern, and a reset input sends it back to its first step.
  The first edge at the clock input takes the pattern over from the internal clock, and the pot then sets a ratio to that clock.
- **One pot, eight amounts.** A selector says which amount the pot sets: the tempo, or how much an LFO at the modulation input moves the note, the gate length, the chance of a step dropping out, which step is played and how loud a note is, the time of the envelope on the DAC's second output, or how far the LFO bends the pitch of the note that is sounding (vibrato, up to two semitones either way).
- **Calibration.** A mode entered at power-on trims the pitch CV against a meter, octave by octave, and stores the result in the EEPROM.
- **Watchdog and crash log.** A watchdog restarts a stuck firmware, and the next start-up records in the EEPROM where the loop had got to.
- **Three builds.** Debug logs each step over serial, midi sends MIDI out in place of the log, and release has neither.

**Most of this has not run on the real board.** It has been checked by host tests and on an emulated board, and most of the hardware it expects is not wired yet.
The outputs are bare logic pins and the DAC's own output, with no buffer or protection.
What the firmware does in detail is in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), and what is left to check on the board in [docs/OPEN_ITEMS.md](docs/OPEN_ITEMS.md).

## Why bare-metal?

It keeps the binary small and the behaviour explicit at the register level.
The compiler is involved in only four places: the `io_low` attribute on the low I/O registers, the `signal` attribute on the one interrupt handler, the builtin for the watchdog's reset instruction, and the section attribute on the one byte of RAM that is kept through a reset.
The fixed-width types come from the compiler's own `<stdint.h>` (the build uses `-ffreestanding`), not from avr-libc.

## Project layout

The code follows a ports-and-adapters layout: the sequencer logic is a pure core with no hardware access, and everything that touches the MCU sits behind small port headers.

```
core/               # Pure sequencer logic; also compiles on a PC
ports/              # What the app needs from the outside world (headers only)
adapters/target/    # The ports implemented for the ATmega328P
adapters/host/      # The ports faked for PC tests
app/                # main, and the loop that wires the ports to the core
tests/              # Host tests, including fake MCU registers
tools/sim/          # Emulated board (make sim, make live)
tools/misra/        # cppcheck MISRA addon config and helper scripts
tools/coverage/     # Coverage report script
tools/check/        # Runs every check and summarizes (make check)
tools/checks.mk     # The Makefile's targets for the checks; tests/tests.mk has the tests'
docs/               # Architecture, open items, roadmap, schematic, parts list
```

The rules are in [CLAUDE.md](CLAUDE.md), with each folder's own rules in a `CLAUDE.md` beside its source and, topic by topic for the core, the MCU adapters and the emulated board, in `.claude/rules/`.
Each source file says what it is for in the comment at its top.

## Hardware target

An Arduino Nano (ATmega328P, 16 MHz) with its stock serial bootloader.
The wiring is drawn pin by pin in [docs/SCHEMATIC.html](docs/SCHEMATIC.html) (open it in a browser), the parts to buy are in [docs/PARTS.md](docs/PARTS.md), and the pin table is in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

- **SPI bus** (D9 to D13): ten 74HC165s for the panel, two 74HC595s for the step lights, and the MCP4822 DAC.
  Each 74HC165's Clock Inhibit pin must be tied to GND.
- **Analog inputs:** the pot on A6 and the modulation input on A7.
- **Digital pins:** gate out on D4, clock out on D5, level light on D6, run button on D7, clock in on D8, reset in on D2.
- **USART** (D1): the log at 115200 baud in the debug build, MIDI out at 31250 baud in the midi build.
- **EEPROM:** 52 bytes, for the calibration record and the crash log.
  A chip erase by an ISP programmer wipes it unless the EESAVE fuse is set; an upload through the bootloader does not.
- **Watchdog:** with the Nano's stock old bootloader a watchdog reset does not recover until the power is cycled; see item 15 in `docs/OPEN_ITEMS.md`.

## Dependencies

Building and flashing the firmware needs nothing beyond one AVR toolchain folder on your `PATH`.
No Arduino IDE, no VS Code extension, and no libraries.

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

Developed on Windows with avr-gcc 12.1.0, GNU Make 4.2.1, and avrdude 7.0.
The `Makefile` is written to work on macOS and Linux as well, but it has not been run on either.

The checks need a few more tools on `PATH`:

| Tool | Needed to |
|---|---|
| A C compiler for your PC (`gcc` on Windows, `cc` elsewhere), with `gcov` | `make test`, `make coverage` |
| `cppcheck` | `make misra` |
| `python` | `make misra`, `make coverage`, `make check`, `make figures` |
| `node` and `npm` (Node.js 22 or later) | `make sim`, `make live`. The first run downloads the [avr8js](https://github.com/wokwi/avr8js) emulator into `tools/sim/node_modules` |

On Windows with Chocolatey, `choco install mingw cppcheck` covers the first two.
`make misra` also needs the MISRA rule headlines file, which is copyrighted and not in this repo; fetch it once with `tools/misra/fetch_misra_headlines.sh`.
Developed with MinGW gcc 16.1.0 and Cppcheck 2.19.0.

## Building and flashing

```sh
make            # Build build/debug/output.hex, then print flash/RAM usage
make size       # Print flash/RAM usage (builds first if needed)
make flash      # Upload build/debug/output.hex to the board
make test       # Build the firmware source for your PC and run the unit tests
make test-quiet # The same, printing one line unless a test fails
make coverage   # Run the tests instrumented; fails unless line and branch coverage is 100%
make misra      # Run cppcheck with the MISRA addon
make sim        # Run the firmware images on an emulated board and check what they do
make live       # Run one image on that board at real speed and watch it in a browser
make check      # The three builds and all four checks above; one line each unless one fails
make figures    # Measure sizes, test counts and coverage, and write docs/FIGURES.md
make file-sizes # List the files grown past the size the rules allow
make clean      # Remove the build/ directory
```

There are three build configurations, chosen with `CONFIG=`.
They differ only in which logger and which MIDI output are linked; every file is compiled the same way for all three.
The chip has one USART, so an image has the log or MIDI out, not both.

| | Logging | MIDI out | Flash | Static RAM |
|---|---|---|---:|---:|
| `make` (same as `CONFIG=debug`) | Text over USART0 at 115200 baud | None | 11996 bytes | 840 bytes |
| `make CONFIG=release` | None | None | 10712 bytes | 278 bytes |
| `make CONFIG=midi` | None | Over USART0 at 31250 baud | 10864 bytes | 312 bytes |

`CONFIG` applies to `make`, `make size` and `make flash` (for example `make flash CONFIG=release`), and each image goes to `build/<config>/output.hex`.
The checks always cover all three.

- **The build** uses `-std=c99 -Wall -Wextra -Wconversion -Wshadow -Werror`, so any warning fails it.
- **`make test`** never touches the board.
  It links the real core and app loop against fake ports, and compiles each MCU adapter against fake registers.
  Time is scripted, so the tests check on which millisecond tick each thing happens.
- **`make coverage`** runs the same tests instrumented and fails unless every line and branch in `core/`, `app/` and `adapters/target/` is exercised.
  That is measured on the PC build: it shows the logic is exercised, not that register addresses or pulse timing are right on the chip.
- **`make sim`** runs the three `output.hex` files, unmodified, on an emulated ATmega328P with models of the shift registers, the DAC, the pot, the jacks and the run button around it, and checks pin timing in CPU cycles.
  It checks the real machine code, which the host tests cannot, but against models of the chip and the parts: the board is still the final check.
- **`make live`** runs one image on the same emulated board at real speed and prints an address (`http://localhost:8165/`; `LIVE_PORT=` changes the port) to open in a browser.
  The page shows the panel, the outputs and the log or MIDI stream, and every control can be worked while it plays.
  It is for looking, not checking: nothing is asserted.
  It serves this PC only, and Ctrl+C stops it.
- **`make check`** runs every stage even if an earlier one failed, and fails if any did.
  Coverage, the analysis and the emulated board run side by side once the builds and the tests are done.
  `python tools/check/run.py make coverage sim` runs only the stages named, with the same one line each.
- **`make flash`** uploads at 57600 baud, which is for the old Nano bootloader; newer boards use 115200.
  It picks the serial port per platform: `COM3` on Windows, the first `/dev/cu.usbserial*`, `/dev/cu.wchusbserial*` or `/dev/cu.usbmodem*` device on macOS, and the first `/dev/ttyUSB*` or `/dev/ttyACM*` device on Linux.
  Override it with, for example, `make flash PORT=COM4`.
  On Linux your user needs access to the port (usually membership of the `dialout` group).

## Coding standard

If you fork this or send changes, write to the same standard.
The firmware source (`core/`, `ports/`, `adapters/` and `app/`) follows BARR-C:2018 (the Barr Group Embedded C Coding Standard) for style and naming, plus these MISRA-style safety rules:

- Use C99 and fixed-width types from `<stdint.h>` for anything with a meaningful size.
- Don't allocate dynamically (no malloc/free) and don't use recursion.
  All memory is static or on the stack, with bounded sizes.
- Every `switch` has a `default`, and there's no fallthrough without a comment.
- Every `if`/`else`/`for`/`while` body uses braces.
- Each function has a single, clear purpose.
  Keep functions short and nesting shallow.
- Don't use implicit conversions that lose data or change signedness.
  Cast explicitly and make sure the cast is justified.
- Check the return value of every function that can fail, or explicitly cast it to `(void)`.
- Use no magic numbers.
  Use named constants or enums.
- Make anything not used outside its file `static`.
  Minimize globals, and mark globals shared with an ISR `volatile` and access them atomically or with interrupts masked.
- Don't use the preprocessor for logic when a `static inline` function or an enum works.
- Document every port function in its header: purpose, units, valid ranges, and whether it's ISR-safe.

Line length: aim for 80 characters and never exceed 100.
Some existing lines are over; new and changed lines should not be.

The checks that back this up are the warning flags the firmware is built with, `make test`, `make coverage`, `make misra` and `make sim`, described above.
MISRA C is a secondary, automated check through cppcheck, which implements only part of it; how findings and deviations are handled is set out in [tools/misra/CLAUDE.md](tools/misra/CLAUDE.md).
Test code in `tests/` and the emulation tooling in `tools/sim/` are outside the standard.

## License

Apache License 2.0 — see [LICENSE](LICENSE).
