---
paths:
  - "adapters/target/level_output.*"
  - "adapters/target/atmega328p_timer0_regs.h"
---

# adapters/target: the level light

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- `level_output.c` implements `level_output_port.h` and owns Timer/Counter0; nothing else includes `atmega328p_timer0_regs.h`.
  The timer runs fast PWM, 8 bits, system clock / 64 (977 Hz), with no interrupt, and its compare output A is PD6.
- Fast PWM cannot give a flat low: a compare value of 0 is still one count high in 256.
  A level of 0 therefore takes the timer off the pin (the compare output bits cleared), and the pin falls back to the port's level, which `register_init.c` left low.
  Nothing may write PD6 in `PORTD`, or a dark light is not dark.
- The compare register is buffered in this mode and taken up as the count returns to 0, so writing it on every tick never cuts a pulse.
- The bit positions and the three addresses were checked against the toolchain's `avr/iom328p.h`, and the emulator gives the duty asked for.
  The pin has not been tried on the board.
