# nano_sequencer roadmap

Written 2026-10-10 from four research passes (existing sequencers, hardware emulation, the output-stage hardware, and Claude Code context cost) and a working emulation spike.
Condensed on 2026-10-11; the fuller wording of every decision is in this file's git history.

**Built so far, as firmware:** the emulated board (`make sim`) and phases 1 to 9, the first piece of phase 10 (MIDI out) and the first stage of the modulation (phase 10a).
**None of it from phase 1 on has run on the board.** Everything else here is still a plan.
What the firmware does today is in ARCHITECTURE.md, not here.

A claim marked **verified** was read from a datasheet or primary manual, or run on this machine.
One marked **unverified** is from memory or a secondary source; check it before relying on it.
[Section 8](#8-not-verified) lists them all.

## Next

Read this section to know what comes next; the rest of the document is the record behind it.
Keep it to a screen, and bring it up to date at the end of each piece of work.

- **Next piece of work: modulation, stage 2, step position.** The LFO offsets the step played, by the amount at selector position 4.
  It touches addressing in every engine.
  Decision 146 has the order of the stages after it: the DAC's channel B as a level and envelope output, vibrato, an LFO of the sequencer's own, profiles with their button, and a status light.
- **After the modulation: MIDI in**, the second piece of phase 10 (decision 117), and then phase 11.
- **To settle before starting stage 2:** whether the offset is in steps of the pattern or places in the range, and what it does in the engines that do not step through sixteen places (stage, Klee, Cartesian).
- **Waiting on the board, not on firmware:** everything from phase 1 on is unrun on hardware.
  Section 8 lists what to check, and OPEN_ITEMS.md the order to check it in.
- **Start each piece of work in a fresh session**, from this section and the decisions it names.
  A long session costs more in every step than anything else does (section 6).

## 1. Summary

- **Where it goes.** A 16-step CV/gate sequencer with one pitch output, gate, clock in and out, reset, and a set of selectable engines that all reinterpret the same 64 panel switches.
  Every engine is pure core logic, so it is cheap to add and fully host-testable.
- **What limits it.** Pins and the single USART, not flash or RAM (ARCHITECTURE.md has the figures).
- **Emulation.** The real firmware images run under an emulator on this machine (section 5), so most of the roadmap is testable without the board, including parts not yet bought.
  The harness is a thin layer over an existing CPU core; it stays in this repo with a clean boundary until a second project needs it.

## 2. Decisions

The 146 decisions taken so far are in [DECISIONS.md](DECISIONS.md), by number.
A "decision N" anywhere in this document is one of those.
Read the ones a piece of work names, not the whole file.

## 3. What the survey found

| Design | Idea worth taking | Fits the 64 switches as |
|---|---|---|
| Klee | A shift register holding many active bits; the output is the sum of the values under them. Three gate buses. Merge turns consecutive active steps into one gate. Invert-on-recirculate doubles the loop | Built otherwise: decisions 86 to 91 |
| Bindubba, René | 16 values addressed as (x, y) by two clocks at different rates | Two internal dividers over the same 16 steps |
| 0-CTRL | Per-step time and strength; step gates patched back into reset, stop and direction | Row B nibble as per-step time or strength; a per-step action table |
| RYK M185, Metropolis | Per-step pulse count and gate mode (hold, repeat, single, rest); ratchets; slide | Steps 1 to 8 as pitch, steps 9 to 16 as 2 bits of gate mode plus 2 bits of pulse count |
| Doepfer A-154 | Direction modes, first and last step, one-shot, deferred reset | A "next address" function in the core |
| Turing Machine | Recirculating bit through a probability gate; lockable loop | Shares the Klee register plus one LFSR |
| Korg SQ-1 | One mode knob reinterprets the second row | The engine selector itself |
| Grids, Euclidean | Fill count spread evenly over a length | A nibble as the fill count |
| ARP 1601 | A gate bus routed to skip or reset | Reserved note values |

Out of scope for this hardware: touch plates, pressure, analog envelopes.

The design consequence: **an engine is a pure function from the panel bits, the engine's own state and the clock to a note, a gate and a next address.**
Adding one is a core file and its tests.

## 4. Architecture as it scales

The layers, the modules, the ports and the tick are built and are described in ARCHITECTURE.md; `core/CLAUDE.md` lists the modules.
What is planned and not yet there:

- **Core modules to come:** a parser for MIDI in, beside `midi_out`; ratchets, slide and swing, in `gate` and `clock`; an LFO of the sequencer's own, an envelope and the profiles (decision 146).
- **Ports are named for what the core needs**, not for the bus.
  No `spi_port`.
- **`adapters/target/` has two levels**: peripheral drivers with headers private to that folder, and device adapters above them that implement ports.
- **Never round the pulse period to whole ticks**; at 300 BPM that is a 4 % tempo error.
  The clock is an integer accumulator for that reason.
- **Each ISR-shared variable** is a file-static `volatile` in exactly one adapter, reached only through a port function.
  The ISR attribute comes from a macro in the register header, so a host test can call the ISR directly.
- **Registers are grouped one header per peripheral**, so a peripheral used by one configuration is not a rule 2.5 finding in another.
- **A further build configuration is another link-time choice**, never a macro.
- **Each ISR brings one cppcheck finding**, rule 8.7 (advisory), a false positive: the handler is referenced from the vector table in the startup object, which the tool does not see.
  Suppress it inline with the reason.

## 5. Emulation

The spike ran the unmodified debug image under avr8js with a modelled 74HC165 chain, pot and UART capture, and every log line matched the panel set in the model over 1249 steps.
One lesson came with it: the first shift register model latched on the falling edge of the load line, where the real chip loads for as long as the line is low.
**A part model encodes one reading of the datasheet, so the board stays the final check.**

| Tool considered | Why not the base |
|---|---|
| simavr | Strong candidate: complete 328P peripherals, VCD, gdb. A Windows build is community-proven but untested here. Worth a spike if gdb against the emulator becomes important |
| QEMU AVR | Only the USART and 16-bit timer for this chip. No GPIO or ADC |
| Wokwi CLI | Simulation runs in their cloud, needs a token, 50 free minutes a month, no pulse-count assertions |
| Renode | Excellent for Cortex-M and RISC-V later. No AVR |
| SimulIDE, PICSimLab, Proteus | GUI-first; not scriptable from make |
| avr-tester (Rust) | The closest existing test-first AVR harness. Linux and macOS only |

The harness (`tools/sim/`, `make sim`) is described in ARCHITECTURE.md.
Still to add, each when a phase needs it:

- **VCD dump on failure**, viewable in GTKWave or PulseView.
- **A board file as plain data**, once there is a second board.
- **Pin recorder helpers** such as `count_rises(pin, t0, t1)`, when a scenario wants one.

`lib/edge_flag.js` supplies what avr8js lacks, a flag that latches an edge with its interrupt off.
It was written from the datasheet, so it cannot settle whether a real chip behaves that way (section 8).

What the harness cannot show: analog behaviour (DAC accuracy, settling, tuning), switch bounce unless scripted, and anything where the emulator's timer or ADC model differs from silicon.

## 6. Claude Code context cost

Verified against the Claude Code documentation:

- Root `CLAUDE.md` loads in full every session.
  `@` imports load eagerly, so they organise but do not save.
- `CLAUDE.md` files in subfolders, and `.claude/rules/*.md` with a `paths:` list, load only when a matching file is read or edited.
- A skill costs one description line until it is invoked.
- A subagent starts cold and spends its own tokens.
  The four research agents behind this document used about 554,000 between them; CLAUDE.md now keeps work in the main session.

Done: the split into a short root file and one per folder, with the topics of `core/` and `adapters/target/` in path-scoped rules (decision 9), a quiet `make check` and `make test-quiet`, `make figures` for the sizes and counts, the "Next" section at the top of this file, and one kind of line ending (LF, enforced by `.gitattributes`).

Not done:

- **A fresh session for each piece of work**, started from "Next".
  The session that built the first stage of the modulation did six steps, and its length was its largest cost.
- **Run coverage, the analysis and the emulated board side by side in `make check`**: about 45 s each, so three minutes could be about one.
- **Settle what changes behaviour before the work starts.** Removing the sync switch meant reworking about forty host tests and thirteen scenarios.
- **Deny reads of `build/**`, `*.hex`, `*.o` and `*.gcov`, and deny `make flash`**, in `.claude/settings.json`.
  Whether a read deny also blocks `cat` through the shell is not confirmed.
- **Confirm in a session that the path-scoped rules load** when a file they name is read; it rests on the documentation so far.
- **Skills for the multi-step recipes** (`add-register`, `host-test-adapter`, `new-engine`), if the folder files grow too long.

## 7. Phases

The firmware of every phase down to 10a is done but for MIDI in, and none of it has run on the board.
"Still to do" is hardware unless it says otherwise.

| Phase | Added | Still to do |
|---|---|---|
| 0. Groundwork | `make sim`, the CLAUDE.md split, `make check` | Optiboot on the board (decision 3), which needs an ISP programmer |
| 1. Timebase | Timer2 tick, the clock, a non-blocking loop and log | Run it |
| 2. Addressing and notes | `address`, `note_map`, `panel`, the engine interface | Controls for the pattern (decision 21) |
| 3. Gate and clock out | `gate`, the two outputs on PD4 and PD5 | Buffer stage, jacks |
| 4. SPI and pitch CV | The SPI driver, the 74HC165 chain on it, the MCP4822 adapter, `pitch_cal` | Rewire the chain; buy and wire the DAC; op-amp buffer |
| 5. Calibration and storage | Calibration mode, the EEPROM adapter, the record, the reset cause | Needs the DAC and a meter |
| 6. Clock, reset and run in | The external clock with a ratio, reset, the run button, `transport`, `debounce` | Two input stages to design and build; button, jacks |
| 7. Engines | The mode switches; stage, Klee, chance, Cartesian and Euclidean; `prng` | Ninth 74HC165 |
| 8. Step LEDs | Sixteen lights on the shared load pin | Two 74HC595s, LEDs, resistors |
| 9. Watchdog and crash log | The watchdog, `errlog`, the breadcrumb | Optiboot, and the reset cause taken from it (decision 115, firmware) |
| 10. MIDI | Out: `CONFIG=midi`, the clock, Start, Stop and notes | DIN socket and two 220 Ω resistors. **In is firmware still to write** (decision 117), with an optocoupler |
| 10a. Modulation, stage 1 | The clock choosing itself; `pickup`, `bank`, `mod`; the selector, level light and LFO input | Tenth 74HC165, coded rotary switch, LED, a jack and its input stage |
| 10b. Modulation, later stages | **Firmware still to write** (decision 146) | Buffer and jack for channel B, a pot, a button, an LED |
| 11. Later | **Firmware still to write**: 24 PPQN sync, timing an edge more finely than a tick (Timer1 timestamps), slide, swing and ratchets | — |

Each phase is one or more small commits, leaves every check green, and updates README and ARCHITECTURE in the same piece of work.

### Pin assignment

In use today, but for the 74HC595 on PB1 and PB3 (not wired) and the proposals marked.

| Pin | Use | Pin | Use |
|---|---|---|---|
| PD0, PD1 | UART (log or MIDI) | PB0 (ICP1) | Clock in |
| PD2 (INT0) | Reset in | PB1 | 74HC165 load and 74HC595 latch |
| PD3 (INT1) | Free; proposed: a status light (decision 146) | PB2 (SS) | DAC chip select |
| PD4 | Gate out | PB3 (MOSI) | DAC and 595 data |
| PD5 | Clock out | PB4 (MISO) | 74HC165 data, through 1 kΩ |
| PD6 (OC0A) | Level light, by pulse width | PB5 (SCK) | SPI clock (on-board LED flickers) |
| PD7 | Run/stop button | PC0 to PC3 | Proposed: CV in, pots; PC0 the internal LFO's rate |
| ADC6 | The pot | PC4, PC5 | Reserved for I2C |
| ADC7 | Modulation input | | |

Where the 328P runs out, in order: one USART (log or MIDI, not both), no on-chip debug, a 10-bit ADC (fine for quantised transpose, not for pitch tracking), three timers.
The smallest step up is the ATmega328PB (two USARTs, same register model); any move costs a new adapter folder and leaves the core and ports alone.

### Panel hardware options

Noted while the live view was redrawn as a panel that could be built.
Nothing here is built or bought, and the live view draws only the first row of each table.
"None" under firmware means the part feeds the same 74HC165 inputs the same bits as today.
ADC7, PD6 and the pot are now taken (decisions 133 and 144), so an option that asks for one needs PC0 to PC3 or a selector position; the tenth 74HC165 has five spare inputs for anything that is a switch.

**Engine selector** (mode inputs 2 to 0)

| Option | Firmware | Verdict |
|---|---|---|
| Binary-coded rotary switch, 8 positions | None | **In the live view.** One move a change. Between detents it can read as another position for a moment, and an engine seen for even one step starts the next from its beginning |
| Gray-coded rotary switch | A lookup from code to engine, in the core | The clean form: only one bit moves between positions. Worth it if the binary one glitches on the bench |
| One-pole 8-way switch with a 74HC148 encoder or seven diodes | None | For when a coded switch cannot be had with the shaft or feel wanted. Choose a non-shorting switch |
| Stepped pot on an ADC channel | A port and adapter, and bands with hysteresis in the core | Frees three switch inputs for the pattern controls (open item 6), at the price of a pin and a reading that can sit on a boundary |
| Three toggles | None | Two moves for most changes, passing through another engine on the way |

**Mode switches 7 to 3** (the chosen engine's own, decision 85)

| Option | Firmware | Verdict |
|---|---|---|
| Five toggles and a printed legend | None | **In the live view**, which draws the legend live. On a panel it is a printed table of five rows, one an engine |
| Two 4-way coded rotary switches and one toggle | The engines' fields moved to line up as 2 + 2 + 1 bits | Better to work, but the fields do not line up today (Klee 2 + 1 + 1 + 1, chance 2 + 1, Cartesian 2 + 2, Euclidean 4) and Euclidean does not fit |
| Two parameter pots on ADC channels | A port and adapter; each engine reads two values and bands them | The best feel, and frees five inputs. A larger change: the engines' interfaces move from switch bits to values |
| A momentary button for Klee load | Load on a press (decision 88) | Reads better as a button; the other engines would then have a button where they want a toggle |
| A light beside each toggle the engine uses | The core gives a mask of the switches in use; five LEDs and a third 74HC595 | Cheap, and no engine changes |

**Steps**

| Option | Firmware | Verdict |
|---|---|---|
| Two rows of eight | None | **In the live view.** The stage engine's controls sit under their stages and each half row is a row of the Cartesian grid |
| A hexadecimal thumbwheel or coded rotary switch a step | None if the code is true binary | Shows the note as a figure and takes far less panel. Hides the Klee and stage engines' reading of the four switches as separate things |
| Two-colour step lights | The core gives a second mask; sixteen more LEDs and two more 74HC595s | Would tell a stage from its control. Not needed for anything built so far |

**Tempo pot and the inputs**

| Option | Firmware | Verdict |
|---|---|---|
| The pot as it is | None | On the external clock its seven bands have no detents and no hysteresis, so the ratio can flicker at a boundary |
| A second, stepped control for the ratio | A second reading and a port for it | Leaves the pot one job, undoing decision 61 for a clearer panel |
| Swing made by the sequencer | Phase 11 | A core feature that needs a control: a selector position or a spare input |
| The clock and reset input stage | None | Not designed (open item 12). Edges arrive at any spacing: the stage should square them up, and the firmware already takes one of any width |

## 8. Not verified

Check these before relying on them.
Each "on the board" item has the order to check things in under its number in OPEN_ITEMS.md.

**Verified since first listed**, against the toolchain's `iom328p.h` and by the emulator running from them: the register addresses and bit positions in use for Timer2, Timer/Counter0, Timer/Counter1's capture, the external interrupt, the SPI, the EEPROM, MCUSR and the watchdog; `__vector_7` as the Timer2 compare A symbol.
Against the datasheets: the MCP4822's frame layout, pinout and SPI mode 0 (DS20002249B), and the 74HC165 and 74HC595 pinouts (SCLS116H, SCLS041J).
That header ships with the toolchain and is a quick check for anything new.

Still open:

- **The OCR1A address**, and any register bit not yet in use.
  Addresses, prescaler codes and vector numbers were read from the datasheet; bit positions were not.
- **Edge flags latching with their interrupts disabled.** The firmware polls ICF1 and INTF0 with their interrupts off.
  The datasheet's wording supports it, but that is from memory and has not been tried, and the emulator is no evidence, because its model was written for this project.
  Also from memory: that the input capture works with nothing more set up than `clock_input_init()` does.
  (Phase 6, open item 12)
- **The input stage for the clock and reset jacks**: not designed.
  The firmware assumes it inverts.
  The MCU's pull-up is 20 to 50 kΩ from memory.
  (Phase 6)
- **The clock, reset and run button on the board.** The 20 ms debounce, the seven-part ratio with no hysteresis, the 1 to 2 ms from an edge to its step, and reset arriving with or just after its clock edge are the things to look at.
  (Phase 6)
- **The Klee, Turing Machine, Cartesian, Euclidean and stage engines' behaviour** is built from section 3's summaries, not checked against the manuals.
  In particular: which way the Klee register shifts, what its merge does at the end of a run, how the Turing Machine weights its probability, and what the M185's hold does at a stage boundary.
  Nothing has been tried on the board.
  (Phase 7, open item 13)
- **SPI mode 0 for the 74HC165, and one pin as both 165 load and 595 latch.** The first board run after rewiring is the check: read a panel with a single switch on and confirm it lands on the right step and bit.
  The 74HC595's absolute maximums are 70 mA through VCC or GND and 35 mA an output.
  (Phases 4 and 8, open item 14)
- **The MCP4822 on the board.** A meter on the output is the check of the gain and enable bits.
  Two things the datasheet added: a frame that runs over 16 clocks has its first 16 taken, and from power-on until the first write the output is off, at about 500 kΩ, not at 0 V. (Phase 4, open item 10)
- **Calibration on the board**, which needs the DAC first: that the pot holds a value to a millivolt while the switch is flipped, that ±64 mV is enough range, and that a bouncing store switch does no harm.
  (Phase 5, open item 11)
- **How long the EEPROM takes.** 3.4 ms a byte is the datasheet's typical figure, from memory.
  Nothing depends on it.
- **What the reset cause reads on this board.** The old bootloader may or may not leave MCUSR alone, and Optiboot, from memory, clears it and passes the value on in a register.
  Read the first log line after a power-up and after pressing reset; if both say 0, the bootloader is taking it and the crash log records nothing (decision 115).
  (Phases 5 and 9)
- **Fuse values, the Nano's brown-out level, and how Optiboot passes on the reset cause.** Check against the Arduino board definitions before burning anything.
  (Phase 9)
- **The breadcrumb on a real chip**: that a byte in `.noinit` keeps its value through a watchdog reset.
  Whether a serial bootloader can read the EEPROM back, to get at the older crash records, has not been looked at.
  (Phase 9)
- **The gate and clock outputs on the board.** Check one period and one width on D4 and D5 with a logic analyser.
  The output stage is not designed.
  (Phase 3)
- **avr8js accuracy for timers and interrupts.** It gives an 8.000 ms scan period and a 100 ms step at 150 BPM; compare one against the board.
- **MIDI electrical values.** The out circuit is taken to be pin 4 of the DIN socket through 220 Ω to +5 V, pin 5 through 220 Ω from TX (PD1), pin 2 to ground, the shield unconnected: from memory of the MIDI 1.0 specification.
  PD1 also feeds the USB serial chip, so the PC's port shows noise in the midi image, and it floats from reset until the USART is enabled.
  For in: the optocoupler circuit's values, and that the opto must be disconnected to upload.
  (Phase 10, open item 16)
- **The modulation input.** No input stage is designed.
  The firmware takes half of AVcc as no modulation and assumes the stage puts an LFO's centre and an empty jack there.
  How far AVcc on USB moves that centre, and whether the ADC reads a second channel cleanly straight after the pot's, are for the board.
  (Phase 10a)
- **The level light on the board**, and that a level of 0 with the compare output taken off the pin is a pin held low.
  (Phase 10a)
- **The selector.** That a coded rotary switch gives true binary with a 1 as a high input, and what it reads between detents, depend on the part bought.
  (Phase 10a)
- **The clock choosing itself, the pickup and the modulation on the board.** (Phase 10a, open items 19 to 22)
- **Survey items read only from retailer or secondary pages**: Moog 960, ARP 1601, Buchla 245/246/248, Serge, René, Turing Machine.
  The Klee, Bindubba, 0-CTRL, M185 and A-154 descriptions are from their manuals.
- **simavr on Windows.** Not tried.

## 9. Sources

- Klee: https://electro-music.com/forum/phpbb-files/know_the_klee_draft4_198.pdf
- Bindubba: https://www.nonlinearcircuits.com/modules/p/bindubba
- 0-CTRL: https://www.makenoise-manuals.com/0-ctrl/0-ctrl-manual.pdf
- RYK M185: https://analoguehaven.com/ryk-modular/m185/manual.pdf
- Doepfer A-154: https://analoguehaven.com/doepfer/a-154/manual.pdf
- Doepfer A-100 signal levels: https://doepfer.de/a100_man/a100t_e.htm
- ATmega328P datasheet (Atmel-42735B)
- MCP4802/4812/4822 datasheet (DS20002249B): https://ww1.microchip.com/downloads/en/DeviceDoc/20002249B.pdf
- SN74HC165 datasheet (SCLS116H): https://www.ti.com/lit/ds/symlink/sn74hc165.pdf
- Arduino Nano pinout: https://docs.arduino.cc/resources/pinouts/A000005-full-pinout.pdf
- Old Nano bootloader source: https://raw.githubusercontent.com/arduino/ArduinoCore-avr/master/bootloaders/atmega/ATmegaBOOT_168.c
- avr8js: https://github.com/wokwi/avr8js — rp2040js: https://github.com/wokwi/rp2040js
- simavr: https://github.com/buserror/simavr
- Renode: https://github.com/renode/renode
- Wokwi CI: https://docs.wokwi.com/wokwi-ci/getting-started
- avr-tester: https://codeberg.org/pwy/avr-tester
- Claude Code memory, skills, subagents, hooks and costs: https://code.claude.com/docs/en/memory , /skills , /sub-agents , /hooks , /costs
- Books: Grenning, *Test-Driven Development for Embedded C*; White, *Making Embedded Systems*; Pont, *Patterns for Time-Triggered Embedded Systems*; Samek, *Practical UML Statecharts in C/C++*
