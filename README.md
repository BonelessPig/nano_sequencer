# nano_sequencer

A bare-metal firmware project for the ATmega328P (Arduino Nano), written from scratch without the Arduino core or avr-libc. All peripheral access goes through manually-defined memory-mapped registers, and standard library pieces normally pulled from avr-libc (`memset`, `memmove`, a minimal `vsnprintf`-based logger) are implemented directly in this repo.

## Status

Work in progress. Currently the firmware initializes the ADC, USART, and I/O direction registers, then continuously samples 6 analog input channels plus a 7th channel used to modulate a loop delay, logging each reading over serial. The digital output/sequencing logic (driving gates, triggers, or CV out) is not yet implemented.

## Why bare-metal?

No Arduino `Wiring`/HAL layer and no avr-libc dependency — registers are defined directly from the ATmega328P datasheet addresses, and only `avr-gcc`'s built-ins (`__builtin_avr_delay_cycles`, `__builtin_va_*`) are used where the compiler must be involved. This keeps the binary small and the behavior fully explicit at the register level.

## Project layout

```
src/
├── main.c                    # Main loop: ADC sampling + serial logging
├── app/
│   ├── init.c / init.h               # Top-level sequencer init (calls serial + register init)
│   ├── register_init.c / .h          # Register address/bit definitions and I/O setup
│   ├── analog_reader.c / .h          # ADC channel read
│   └── serial_logger.c / .h          # USART setup + printf-style logging with log levels
└── common/
    ├── common_types.h        # Shared status/error codes
    └── utilities.c / .h      # delay_ms, memset, memmove
```

## Hardware target

- MCU: ATmega328P (as used on the Arduino Nano)
- Clock: 16 MHz
- USART: 9600 baud
- Analog inputs: ADC channels 0–5 (sequencer step values), channel 6 (delay/tempo control)
- Digital I/O configured in `register_init.c`: `PORTB5` and `PORTD0` as outputs, `PORTC0` as input, `PORTC1` as output

## Building and flashing

No build system is set up yet — compile and flash manually with the AVR GCC toolchain and `avrdude`:

```sh
avr-gcc -Os -DF_CPU=16000000UL -mmcu=atmega328p -c -o main.o main.c   # Compile
avr-gcc -mmcu=atmega328p main.o -o main                               # Link
avr-objcopy -O ihex -R .eeprom main main.hex                          # Convert to Intel HEX
avrdude -F -V -c arduino -p ATMEGA328P -P COM3 -b 57600 -U flash:w:main.hex   # Upload
```

Adjust `-P COM3` to match the serial port your Nano enumerates as.

## License

Apache License 2.0 — see [LICENSE](LICENSE).
