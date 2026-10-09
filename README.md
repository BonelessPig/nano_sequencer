# nano_sequencer

A bare-metal firmware project for the ATmega328P (Arduino Nano), written from scratch without the Arduino core and without any avr-libc headers or library functions. All peripheral access goes through manually-defined memory-mapped registers, and standard library pieces normally pulled from avr-libc (`memset`, `memmove`, a minimal `vsnprintf`-based logger) are implemented directly in this repo. The only code not written here is what the toolchain adds at link time: the chip's startup object (vector table and stack setup, which ships with avr-libc) and a few `libgcc` helpers.

## Status

Work in progress. Currently the firmware initializes the ADC, USART, and I/O direction registers, then continuously reads 16 step note values from a daisy-chained 74HC165 shift register bank plus an analog channel used to modulate the inter-step delay, logging each reading over serial. The digital output/sequencing logic (driving gates, triggers, or CV out) is not yet implemented.

## Why bare-metal?

No Arduino `Wiring`/HAL layer and no avr-libc headers or functions in the source — registers are defined directly from the ATmega328P datasheet addresses, and only `avr-gcc`'s built-ins (`__builtin_avr_delay_cycles`, `__builtin_va_*`) are used where the compiler must be involved. This keeps the binary small and the behavior fully explicit at the register level.

## Project layout

```
src/
├── main.c                    # Main loop: read step notes + tempo pot, log over serial
├── app/
│   ├── init.c / init.h               # Top-level sequencer init (calls serial + register init)
│   ├── register_init.c / .h          # I/O direction + ADC setup
│   ├── analog_reader.c / .h          # ADC channel read
│   ├── shift_reg_reader.c / .h       # 74HC165 shift register chain read (16 step notes)
│   └── serial_logger.c / .h          # USART setup + printf-style logging with log levels
├── common/
│   ├── common_types.h        # Shared status/error codes
│   ├── bits.h                 # BIT_0..BIT_7 mask constants
│   ├── varargs.h             # va_list macros built on compiler builtins
│   └── utilities.c / .h      # delay_ms, memset, memmove, minimal vsnprintf
└── mcu/
    └── atmega328p_regs.h     # Memory-mapped register addresses and bit positions
```

## Hardware target

- MCU: ATmega328P (as used on the Arduino Nano)
- Clock: 16 MHz
- USART: 9600 baud
- Analog inputs: ADC channel 6 (delay/tempo control)
- Step notes: 16 steps × 4 bits, read from a chain of 8 daisy-chained 74HC165 shift registers via `PORTD2` (SH/LD), `PORTD3` (CLK), and `PORTD4` (SER data-in) — each 74HC165's Clock Inhibit/CE pin must be tied to GND in hardware
- Digital I/O configured in `register_init.c`: `PORTB5`, `PORTD0`, `PORTD2`, `PORTD3` as outputs, `PORTC0`, `PORTD4` as inputs, `PORTC1` as output

## Dependencies

Nothing needs to be installed beyond one AVR toolchain folder on your `PATH`. No Arduino IDE, no VS Code extension, and no libraries.

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

On the hardware side: an Arduino Nano (ATmega328P, 16 MHz) with its stock serial bootloader, and a USB cable. The upload speed in the `Makefile` (57600 baud) is for the old Nano bootloader; newer boards use 115200.

## Building and flashing

Build and flash with the included `Makefile`:

```sh
make            # Compile + link + convert to build/output.hex, then print flash/RAM usage
make size       # Print flash/RAM usage (builds first if needed)
make flash      # Flash build/output.hex
make clean      # Remove the build/ directory
```

`make flash` picks the serial port per platform: `COM4` on Windows, the first `/dev/cu.usbserial*`, `/dev/cu.wchusbserial*` or `/dev/cu.usbmodem*` device on macOS, and the first `/dev/ttyUSB*` or `/dev/ttyACM*` device on Linux. Override it if your Nano enumerates differently, e.g. `make flash PORT=COM3` or `make flash PORT=/dev/ttyUSB1`. On Linux your user needs access to the port (usually membership of the `dialout` group).

## License

Apache License 2.0 — see [LICENSE](LICENSE).
