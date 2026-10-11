# nano_sequencer parts list

What it takes to build the whole board the firmware is written for. The wiring is in [SCHEMATIC.html](SCHEMATIC.html), and what each part does in [ARCHITECTURE.md](ARCHITECTURE.md).

Where the documents do not fix a value, this says so. Those are typical choices, not something the firmware depends on.

## Why two kinds of shift register

They do opposite jobs, so one part cannot stand in for the other:

- **74HC165** is parallel-in, serial-out. It reads the 72 panel switches and shifts them to the MCU on one wire.
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
| 9 | 74HC165 | 8 for the steps, 1 for the mode switches |
| 72 | Toggle switches (SPST is enough) | 64 step switches, 8 mode switches |
| 72 | Pull-down resistors, about 10 kΩ (or 9 eight-way resistor networks) | One per switch input. Value not fixed |
| 1 | 1 kΩ resistor | Between the chain's output and D12, so a programmer can still override the line |
| 1 | Potentiometer, linear, about 10 kΩ | Tempo. Value not fixed |
| 1 | Momentary push button | Run and stop |
| 1 | Toggle switch | Sync: the internal clock or the external one |

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
| 5 | Jack sockets (3.5 mm for Eurorack) | Clock in, reset in, gate out, clock out, CV out |
| 2 | NPN small-signal transistors (a 2N3904, say) | One inverting input stage per input jack |
| 2 | 1N4148 diodes | Ground to base, protecting each transistor from negative voltages |
| 2 | Base resistors | Jack to base. Value not chosen |
| 2 | 10 kΩ resistors, optional | A stiffer pull-up on each input pin |
| 1 | Buffer IC, plus series resistors | Gate and clock outputs |

### Everywhere

| Qty | Part | For |
|---|---|---|
| 12 or more | 100 nF ceramic capacitors | One at each chip's supply pin |
| 1 | Bulk capacitor, 10 to 100 µF | Across the 5 V rail. Not in the other documents, but sensible with 12 chips |

### MIDI (roadmap phase 10, not built)

| Qty | Part | For |
|---|---|---|
| 2 | 5-pin DIN sockets | In and out |
| 1 | Optocoupler (the 6N138 is the usual one) | MIDI in |
| A few | Resistors and a diode | Around the sockets and the optocoupler. Values not verified |

## Not designed yet

Three parts of this list are placeholders, because the circuits behind them have not been worked out:

- **The two input stages.** The firmware assumes they invert, and nothing more is decided.
- **The gate and clock output buffer.** The pins are bare 0 V and 5 V logic today, with no protection.
- **The CV buffer and its supply.** The DAC gives 0 to 3.75 V. An op-amp running from the same 5 V rail must be rail-to-rail to reach 0 V. See "Power" below.

## Power

The board runs from USB for now. A Eurorack supply is a possible later step, so these stages are to be designed to work from either (roadmap decision 116). What that means for the parts:

- **The CV buffer** gets a rail-to-rail op-amp that runs from a single 5 V rail (an MCP6002, say), which reaches 0 V there and would also do on a Eurorack board. A TL072 only works from ±12 V, so it is not the part to buy now.
- **The gate and clock outputs** are 5 V from USB. That usually triggers other modules, though many expect more; a higher level is only possible from the Eurorack rails, so leave room for it in the output stage.
- **A Eurorack build adds** a 10-pin or 16-pin power header, a 5 V regulator from +12 V, and protection diodes on the rails. The regulator and the Nano's USB socket must not both feed the 5 V rail at once without a diode or a jumper to keep them apart.

None of this is designed in detail, and the op-amp named is a typical choice, not a checked one.
