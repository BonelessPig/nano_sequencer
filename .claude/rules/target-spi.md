---
paths:
  - "adapters/target/spi.*"
  - "adapters/target/shift_reg_reader.*"
  - "adapters/target/cv_output.*"
  - "adapters/target/atmega328p_spi_regs.h"
---

# adapters/target: the SPI bus and the parts on it

Loaded with the files below, on top of `adapters/target/CLAUDE.md`, whose rules apply to all of `adapters/target/`.

- A chip select is released on every path out of a write, failures included.
  A part that ignores a short frame (the MCP4822) is then left as it was.
- `shift_reg_reader.c` implements two ports, the step input and the step lights, because both chains hang on PB1: it is the 74HC165s' load line and the 74HC595s' latch clock.
  Nothing else may move PB1.
  The lights' two bytes are sent with the line idle, immediately before the load pulse whose rising edge latches them; keep anything else off the bus between the two, or it is what gets latched.
  `led_output_write()` only stores the value.
- A part with no chip select (the 74HC165 chain) is clocked by every transfer on the bus.
  That is harmless only because it is loaded afresh before each read; keep it so.
- The SPI is master, mode 0, MSB first, 1 MHz.
  Every part on the bus must work in that mode, or the mode becomes a parameter of the transfer.
  SS (PB2) stays an output: as a low input it cancels master mode.
- The 74HC165 chain is ten chips: eight for the steps, one for the mode switches and, farthest from the MCU, one whose low three inputs are the selector.
  Another chip goes on the far end, so the bytes already read keep their places; it costs a constant in `shift_reg_reader.c`, the port header's range and the two fakes.
- The MCP4822 is used on channel A at a gain of 2 from its internal reference (1 mV a step), with LDAC tied low.
  Its frame layout and pinout have been checked against the datasheet (`docs/ROADMAP.md` section 8).
