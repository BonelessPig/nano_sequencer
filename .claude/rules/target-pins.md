---
paths:
  - "adapters/target/register_init.*"
  - "adapters/target/gate_output.*"
  - "adapters/target/clock_output.*"
---

# adapters/target: pins

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- In use: PB1 (74HC165 load), PB5 (SCK, the clock of the chain and the DAC), PB4 (MISO, the chain's data, through 1 kΩ), PB3 (MOSI, the data of the DAC and of the step lights' 74HC595s), PB2 (SS, the DAC's chip select, idle high), PD4 (gate output), PD5 (clock output), ADC channel 6 (the pot), ADC channel 7 (the modulation input), PB0 (clock input), PD2 (reset input), PD7 (run button), and PD1 as the USART's transmit pin: the log in the debug build, MIDI out in the midi build (PD0 is then its receive input, which nothing reads).
  Timer/Counter2 is the 1 kHz tick and drives no pin, and the EEPROM has none either.
  Timer/Counter1 runs only for its capture flag, and Timer/Counter0 only for the level light.
- PD3 is free, and left an input with its pull-up on so that it does not float.
  PD6 is the level light, driven by Timer/Counter0.
  PC0 and PC1 are given a direction but are unused and free.
- An input pin's direction and pull-up are set in `register_init.c` too.
  All three inputs use the MCU's own pull-ups.
- An output pin's direction and idle level are set in `register_init.c`, level first.
  The adapter that writes it (`gate_output.c`, `clock_output.c`) only changes its level, one pin per statement so that it compiles to a single `sbi` or `cbi` and cannot disturb the rest of the port.
  Check that with `avr-objdump -d` for a new one.
- The gate and clock outputs are active high.
  If a buffer stage inverts them, invert in those two files and nowhere else.
- PB3 to PB5 are also the ISP programmer's pins; the 1 kΩ on MISO is what lets a programmer override the chain.
