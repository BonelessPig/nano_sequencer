---
paths:
  - "adapters/target/*logger*"
  - "adapters/target/*midi_output*"
  - "adapters/target/atmega328p_usart_regs.h"
---

# adapters/target: loggers, MIDI outputs and the UART

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- There is one USART and two link-time pairs that could want it: the logger (`serial_logger.c` or `null_logger.c`) and the MIDI output (`midi_output.c` or `null_midi_output.c`).
  The Makefile links one of each, and at most one file across both pairs may touch the USART: the serial logger in debug, the MIDI output in midi, neither in release.
  A new configuration is a new choice from the pairs, never a macro.
- `serial_logger.c` (debug) and `null_logger.c` (release and midi) both implement `log_port.h` and `logger.h`.
  A change to either interface changes both files.
  The same goes for `midi_output.c` (midi) and `null_midi_output.c` (debug and release), which implement `midi_output_port.h` and `midi_output.h`.
- `null_midi_output.c` never touches a register and gives a room of 255.
  That is what keeps the release build off PD0 and PD1.
- MIDI's 31250 baud is exact in double-speed mode at 16 MHz, a divisor of 63.
  A byte is 320 µs, so about 3 leave in a tick against up to 8 queued in one; the 32-byte queue is four worst-case ticks.
  `midi_output_write()` only queues and `midi_output_poll()` sends one byte per call when the transmitter is ready.
- A port function that calls a sibling port function keeps a rule 8.7 finding alive for the one called.
  Both use a static helper instead (`queue_room()` behind `midi_output_room()` and `midi_output_write()`).
- `midi_output_init()` returns nothing, since it cannot fail, and goes with the driver inits in `platform_init()`, before the watchdog.
- PD1 floats from reset until the transmitter is enabled.
  That matters for the MIDI out circuit and has not been tried (`docs/ARCHITECTURE.md` open item 16).
- The serial logger never waits for the UART.
  A message is written into a 128-byte queue and `log_poll()` sends one byte per call when the transmitter is ready.
  A message that does not fit is dropped whole, so write every message between `begin_message()` and `end_message()`.
  Only `log_flush()` blocks.
- The serial rate is 115200 baud in double-speed mode, which is 2.1 % fast at 16 MHz.
  250000, 500000 and 1000000 are exact.
- The release build never enables the USART.
  PD0 and PD1 are not configured by `register_init.c`; leave them alone so the release build does not drive them against the board's USB serial chip.
  In the midi build PD1 also reaches that chip, so the PC's port shows the MIDI bytes as noise.
