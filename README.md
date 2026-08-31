# nano_sequencer

A bare-metal firmware project for the ATmega328P (Arduino Nano), written from scratch without the Arduino core or avr-libc. All peripheral access goes through manually-defined memory-mapped registers, and standard library pieces normally pulled from avr-libc (`memset`, `memmove`, a minimal `vsnprintf`-based logger) are implemented directly in this repo.

## Status

Work in progress. Currently the firmware initializes the ADC, USART, and I/O direction registers, then continuously reads 16 step note values from a daisy-chained 74HC165 shift register bank plus an analog channel used to modulate the inter-step delay, logging each reading over serial. The digital output/sequencing logic (driving gates, triggers, or CV out) is not yet implemented.

## Why bare-metal?

No Arduino `Wiring`/HAL layer and no avr-libc dependency — registers are defined directly from the ATmega328P datasheet addresses, and only `avr-gcc`'s built-ins (`__builtin_avr_delay_cycles`, `__builtin_va_*`) are used where the compiler must be involved. This keeps the binary small and the behavior fully explicit at the register level.

## Project layout

```
src/
├── main.c                    # Main loop: ADC sampling + serial logging
├── app/
│   ├── init.c / init.h               # Top-level sequencer init (calls serial + register init)
│   ├── register_init.c / .h          # I/O direction + ADC setup
│   ├── analog_reader.c / .h          # ADC channel read
│   ├── shift_reg_reader.c / .h       # 74HC165 shift register chain read (16 step notes)
│   └── serial_logger.c / .h          # USART setup + printf-style logging with log levels
├── common/
│   ├── common_types.h        # Shared status/error codes
│   ├── bits.h                 # BIT_0..BIT_7 mask constants
│   └── utilities.c / .h      # delay_ms, memset, memmove
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

## Building and flashing

Build and flash with the included `Makefile` (requires `avr-gcc`, `avr-objcopy`, and `avrdude` on your `PATH`):

```sh
make            # Compile + link + convert to build/output.hex
make flash      # Flash build/output.hex (defaults to PORT=COM4)
make clean      # Remove the build/ directory
```

Override the port if your Nano enumerates differently, e.g. `make flash PORT=COM3`.

## License

Apache License 2.0 — see [LICENSE](LICENSE).
