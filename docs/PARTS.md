# nano_sequencer parts list

What it takes to build the whole board the firmware is written for. The wiring is in [SCHEMATIC.html](SCHEMATIC.html), and what each part does in [ARCHITECTURE.md](ARCHITECTURE.md).

Where the documents do not fix a value, this says so. Those are typical choices, not something the firmware depends on.

## Why two kinds of shift register

They do opposite jobs, so one part cannot stand in for the other:

- **74HC165** is parallel-in, serial-out. It reads the 72 panel switches and the selector and shifts them to the MCU on one wire.
- **74HC595** is serial-in, parallel-out. It takes two bytes from the MCU and holds 16 outputs steady to drive the step lights.

Neither chip works in the other direction. They share the SPI clock, and one pin (D9) serves as both the 165s' load line and the 595s' latch, so the pair costs no extra MCU pins.

## Parts

### Core

| Qty | Part | For |
|---|---|---|
| 1 | Arduino Nano (ATmega328P, 16 MHz) | The MCU |
| 1 | ISP programmer (a USBasp, say) | Burning Optiboot. A tool, not on the board |

### Panel inputs

| Qty | Part | For |
|---|---|---|
| 10 | 74HC165 | 8 for the steps, 1 for the mode switches, 1 for the selector |
| 72 | Toggle switches (SPST is enough) | 64 step switches, 8 mode switches |
| 75 | Pull-down resistors, about 10 kΩ (or 10 eight-way resistor networks) | One per switch input, and one for each of the selector's three lines. The tenth chip's five spare inputs go to ground. Value not fixed |
| 1 | 1 kΩ resistor | Between the chain's output and D12, so a programmer can still override the line |
| 1 | Potentiometer, linear, about 10 kΩ | Sets whichever amount the selector is on: the tempo at position 0. Value not fixed |
| 1 | Rotary switch, 8 positions, binary coded (real code, with a common that goes to 5 V) | The selector: which amount the pot sets |
| 1 | LED and resistor | The level light on D6: as bright as the amount the selector is on |
| 1 | Momentary push button | Run and stop |

### Step lights

| Qty | Part | For |
|---|---|---|
| 2 | 74HC595 | 16 outputs |
| 16 | LEDs | One per step |
| 16 | Resistors, 470 Ω to 1 kΩ | One per LED. To be chosen for the LEDs, at a few milliamps each |

### Pitch CV

| Qty | Part | For |
|---|---|---|
| 1 | MCP4822 (DIP-8) | The DAC |
| 1 | Op-amp, rail-to-rail, for a single 5 V supply | Buffer between the DAC and the jack. See "Power" |

### Jacks and their stages

| Qty | Part | For |
|---|---|---|
| 6 | Jack sockets (3.5 mm for Eurorack) | Clock in, reset in, modulation in, gate out, clock out, CV out |
| 1 | Op-amp stage, or a resistor network with clamp diodes | The modulation input: an LFO scaled and shifted so that its centre sits at half of 5 V. Not designed |
| 2 | NPN small-signal transistors (a 2N3904, say) | One inverting input stage per input jack |
| 2 | 1N4148 diodes | Ground to base, protecting each transistor from negative voltages |
| 2 | Base resistors | Jack to base. Value not chosen |
| 2 | 10 kΩ resistors, optional | A stiffer pull-up on each input pin |
| 1 | Buffer IC, plus series resistors | Gate and clock outputs |

### Everywhere

| Qty | Part | For |
|---|---|---|
| 13 or more | 100 nF ceramic capacitors | One at each chip's supply pin |
| 1 | Bulk capacitor, 10 to 100 µF | Across the 5 V rail. Not in the other documents, but sensible with 13 chips |

### MIDI out (roadmap phase 10; the firmware is built, the circuit is not)

| Qty | Part | For |
|---|---|---|
| 1 | 5-pin DIN socket | MIDI out. Its shield is left unconnected |
| 2 | 220 Ω resistors | One from +5 V to pin 4, one from D1 (TX) to pin 5; pin 2 goes to ground. From memory of the MIDI specification, not verified (roadmap section 8) |

### MIDI in (roadmap phase 10, not built)

| Qty | Part | For |
|---|---|---|
| 1 | 5-pin DIN socket | MIDI in |
| 1 | Optocoupler (the 6N138 is the usual one) | Isolating the input |
| A few | Resistors and a diode | Around the socket and the optocoupler. Values not verified |

## Not designed yet

Four parts of this list are placeholders, because the circuits behind them have not been worked out:

- **The two input stages.** The firmware assumes they invert, and nothing more is decided.
- **The gate and clock output buffer.** The pins are bare 0 V and 5 V logic today, with no protection.
- **The modulation input stage.** The firmware takes half of 5 V at the pin as an LFO at its centre or no LFO at all, to within about 78 mV. What scales and shifts a ±5 V LFO to that, and protects the pin, is not worked out. If it uses an op-amp, the second half of the CV buffer's is free.
- **The CV buffer and its supply.** The DAC gives 0 to 3.75 V. An op-amp running from the same 5 V rail must be rail-to-rail to reach 0 V. See "Power" below.

## Power

The board runs from USB for now. A Eurorack supply is a possible later step, so these stages are to be designed to work from either (roadmap decision 116). What that means for the parts:

- **The CV buffer** gets a rail-to-rail op-amp that runs from a single 5 V rail (an MCP6002, say), which reaches 0 V there and would also do on a Eurorack board. A TL072 only works from ±12 V, so it is not the part to buy now.
- **The gate and clock outputs** are 5 V from USB. That usually triggers other modules, though many expect more; a higher level is only possible from the Eurorack rails, so leave room for it in the output stage.
- **A Eurorack build adds** a 10-pin or 16-pin power header, a 5 V regulator from +12 V, and protection diodes on the rails. The regulator and the Nano's USB socket must not both feed the 5 V rail at once without a diode or a jumper to keep them apart.

None of this is designed in detail, and the op-amp named is a typical choice, not a checked one.
