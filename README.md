# nano_sequencer

A bare-metal firmware project for the ATmega328P (Arduino Nano), written from scratch without the Arduino core and without any avr-libc headers or library functions. All peripheral access goes through manually-defined memory-mapped registers, and nothing from the standard library is used: the serial logger does its own number-to-text conversion. The only code not written here is what the toolchain adds at link time: the chip's startup object (vector table and stack setup, which ships with avr-libc) and a few `libgcc` helpers.

## Status

Work in progress. Currently the firmware initializes the ADC, USART, and I/O direction registers, then continuously reads 16 step note values from a daisy-chained 74HC165 shift register bank plus an analog channel used to modulate the inter-step delay, logging each reading over serial. The digital output/sequencing logic (driving gates, triggers, or CV out) is not yet implemented.

## Why bare-metal?

No Arduino `Wiring`/HAL layer and no avr-libc headers or functions in the source — registers are defined directly from the ATmega328P datasheet addresses, and only one `avr-gcc` built-in (`__builtin_avr_delay_cycles`) is used where the compiler must be involved. The fixed-width types come from the compiler's own `<stdint.h>` (the build uses `-ffreestanding`), not from avr-libc. This keeps the binary small and the behavior fully explicit at the register level.

## Project layout

The code follows a ports-and-adapters layout: the sequencer logic is a pure core with no hardware access, and everything that touches the MCU sits behind small port headers. The rules are in [CLAUDE.md](CLAUDE.md) and the details in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

```
core/                         # Pure sequencer logic; also compiles on a PC
└── seq.c / seq.h                     # seq_tick(): raw inputs in, notes + delay out
ports/                        # What the app needs from the outside world (headers only)
├── port_status.h                     # Shared status/error codes
├── platform_port.h                   # One-time platform bring-up
├── step_input_port.h                 # Raw step-note bits
├── tempo_input_port.h                # Raw tempo control position
├── delay_port.h                      # Blocking wait
└── log_port.h                        # Diagnostic output
adapters/
├── target/                   # The ports implemented for the ATmega328P
│   ├── init.c                        # Platform bring-up (serial + registers)
│   ├── register_init.c / .h          # I/O direction + ADC setup
│   ├── analog_reader.c               # Tempo input: ADC channel read
│   ├── shift_reg_reader.c            # Step input: 74HC165 shift register chain read
│   ├── delay.c                       # Delay: calibrated busy-wait
│   ├── serial_logger.c / .h          # Log: USART setup + text logging with log levels
│   ├── bits.h                        # BIT_0..BIT_7 mask constants
│   └── atmega328p_regs.h             # Memory-mapped register addresses and bit positions
└── host/                     # The ports faked for PC tests
    └── host_ports.c / .h             # Scripted inputs, recorded outputs
app/
├── app.c / app.h                     # Wires ports to the core: gather inputs, tick, apply outputs
└── main.c                            # Init, then the forever loop
tests/                        # Host tests (make test)
tools/misra/                  # cppcheck MISRA addon config and helper scripts
```

## Hardware target

- MCU: ATmega328P (as used on the Arduino Nano)
- Clock: 16 MHz
- USART: 9600 baud
- Analog inputs: ADC channel 6 (delay/tempo control)
- Step notes: 16 steps × 4 bits, read from a chain of 8 daisy-chained 74HC165 shift registers via `PORTD2` (SH/LD), `PORTD3` (CLK), and `PORTD4` (SER data-in) — each 74HC165's Clock Inhibit/CE pin must be tied to GND in hardware
- Digital I/O configured in `register_init.c`: `PORTB5`, `PORTD0`, `PORTD2`, `PORTD3` as outputs, `PORTC0`, `PORTD4` as inputs, `PORTC1` as output

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

For development, two optional checks need extra tools on `PATH`:

| Tool | Used for | Needed to |
|---|---|---|
| A C compiler for your PC (`gcc` on Windows, `cc` elsewhere) | Building the hardware-free code and its tests | `make test` |
| `cppcheck` | Static analysis with its MISRA addon | `make misra` |
| `python` | Running cppcheck's MISRA addon | `make misra` |

On Windows with Chocolatey, `choco install mingw cppcheck` covers the first two. `make misra` also needs the MISRA rule headlines file, which is copyrighted and not in this repo; fetch it once with `tools/misra/fetch_misra_headlines.sh`. Developed with MinGW gcc 16.1.0 and Cppcheck 2.19.0.

On the hardware side: an Arduino Nano (ATmega328P, 16 MHz) with its stock serial bootloader, and a USB cable. The upload speed in the `Makefile` (57600 baud) is for the old Nano bootloader; newer boards use 115200.

## Building and flashing

Build and flash with the included `Makefile`:

```sh
make            # Compile + link + convert to build/output.hex, then print flash/RAM usage
make size       # Print flash/RAM usage (builds first if needed)
make flash      # Flash build/output.hex
make test       # Build the core and app loop for your PC and run the unit tests
make misra      # Run cppcheck with the MISRA addon
make clean      # Remove the build/ directory
```

The firmware is compiled with `-std=c99 -Wall -Wextra -Wconversion -Wshadow -Werror`, so any warning fails the build. `make test` never touches the board: it links the real core and app loop against fake ports and checks what they do, and it runs the serial logger against fake USART registers to check the exact text it sends.

`make flash` picks the serial port per platform: `COM3` on Windows, the first `/dev/cu.usbserial*`, `/dev/cu.wchusbserial*` or `/dev/cu.usbmodem*` device on macOS, and the first `/dev/ttyUSB*` or `/dev/ttyACM*` device on Linux. Override it if your Nano enumerates differently, e.g. `make flash PORT=COM4` or `make flash PORT=/dev/ttyUSB1`. On Linux your user needs access to the port (usually membership of the `dialout` group).

## License

Apache License 2.0 — see [LICENSE](LICENSE).
