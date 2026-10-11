# nano_sequencer roadmap

Written 2026-10-10 from four research passes (existing sequencers, hardware emulation, the output-stage hardware, and Claude Code context cost) plus a working emulation spike, and updated the same day with the owner's decisions (section 2). The emulated board (`make sim`), phase 1 (the timebase), phase 2 (addressing, note mapping and the engine interface), the firmware of phase 3 (the gate and the clock output, as logic pins with no buffer stage yet), the firmware of phase 4 (the SPI driver with the shift registers moved onto it, the pitch CV through an MCP4822 DAC, and pitch calibration with the ideal table) the firmware of phase 5 (calibration mode, the calibration record in the EEPROM, and the reset cause) and the firmware of phase 6 (the clock and reset inputs, the run button, the sync switch, and the external clock with a ratio) are built; everything else here is still a plan.

Each claim that matters is marked **verified** (read from a datasheet or primary manual, or run on this machine), or **unverified** (from memory or a secondary source; check before relying on it). The full list of unverified items is in [section 8](#8-not-verified).

## 1. Summary

- **Where it goes.** A 16-step CV/gate sequencer with one pitch output, gate, clock in and out, reset, and a set of selectable sequencing engines (plain, Metropolis-style, Klee-style, random) that all reinterpret the same 64 panel switches. Every engine is pure core logic, so it is cheap to add and fully host-testable.
- **What limits it.** Pins and the single USART, not flash or RAM (after phase 6 the debug image uses 6194 of 30720 bytes of flash and 624 of 2048 bytes of RAM).
- **Emulation.** The real firmware image already runs under an emulator on this machine (section 5). That makes most of the roadmap testable without the board, including parts you have not bought.
- **A separate emulation project.** The gap is real but shallow: nothing open, local and headless covers AVR plus other MCU families with a board file and waveform assertions. It is a thin layer over existing CPU cores, not a new emulator. Recommendation: build it inside this repo with a clean boundary and extract it when a second project needs it.
- **Claude context.** Splitting CLAUDE.md into a short root file, per-folder files and skills cuts the always-loaded instructions by an estimated 55 to 60 percent, and new per-module guidance then costs nothing until that module is touched (section 6).

## 2. Decisions

Settled on 2026-10-10 unless marked open.

| # | Decision | Outcome |
|---|---|---|
| 1 | Emulator base | **avr8js first**, set up as `make sim`. simavr may be added later for gdb and VCD; its Windows build is still untested. |
| 2 | Emulation harness location | **In this repo** (`tools/sim/`), with `lib/` and `parts/` kept free of anything sequencer-specific so they can be extracted at the second consumer. |
| 3 | Bootloader | **Optiboot for now**, burned by ISP; the final board may have none. The stock old bootloader never disables the watchdog (verified in its source), so this is needed before phase 9. It also changes the upload speed in the Makefile from 57600 to 115200. |
| 4 | Shift registers | **Move the 74HC165 chain to hardware SPI.** Planned for phase 4 and **done ahead of phase 3**, because phase 3 wants PD4 for the gate. Frees INT0 and INT1 and shares the bus with the DAC and the LED chain. Three signals to rewire: load D2 to D9, clock D3 to D13, data D4 to D12 through 1 kΩ. |
| 5 | Output timing | **Apply outputs at the tick boundary**: each pass applies what the previous pass computed, then gathers and computes. A constant 1 ms latency for near-zero jitter. The tick rule in CLAUDE.md changes with phase 1. |
| 6 | Rests | **Note 0 is a rest**, leaving 15 pitches. |
| 7 | Choosing a mode | **A ninth 74HC165** (8 more switches, no MCU pins). The core's raw input grows from 8 to 9 bytes. |
| 8 | Pitch DAC | **MCP4822** (12-bit, SPI, internal 2.048 V reference), settled on 2026-10-10. The others considered were the MCP4725 (I2C, reference is the supply), PWM and R-2R; every one of them takes its reference from the USB 5 V rail, and a 1 % supply change is about 48 cents at 4 V. A 16-bit part (DAC8562/8563, AD5683R/AD5693R) is the upgrade if slides or fine tuning ever want steps finer than 1.2 cents; it would be a new adapter behind the same port. A home-made DAC as a separate project would be the same. |
| 9 | Instruction layout for Claude | **Nested `CLAUDE.md` files per folder.** The C coding standard and a line length rule (80 soft, 100 hard) are also in the user-level `~/.claude/CLAUDE.md`. |

Taken while building phase 1, as the simplest thing that met the plan. Each is one constant or a few lines to change.

| # | Decision | Outcome |
|---|---|---|
| 10 | Tempo range | **30 to 285 BPM, linear**: 30 plus the pot reading divided by 4. A step is a sixteenth note (24 pulses). |
| 11 | Input scan rate | **Every 8 ticks (125 Hz)**, panel and pot together, scheduled by the app. |
| 12 | Log when the queue is full | **Drop the whole message.** The queue is 128 bytes, enough for the two longest messages. Failed reads are logged once per step, not once per scan. |
| 13 | Steps owed after a stall | **Begin one per tick until caught up**, none skipped. |
| 14 | Start-up | **The first tick begins step 0.** It used to count towards step 1 as well, which made step 0 a tick short; phase 6 put that right (decision 71). |

Taken while building phase 2, on the same basis. Each is contained in one core module.

**To reconfirm.** Decisions 15 to 24 were accepted on 2026-10-10 as provisional, to keep the work moving, and have not been reviewed one by one. Go through them again before any of these controls gets hardware (phase 7), since that is when they become audible and when changing them stops being free. Reset got its input in phase 6: the owner was asked whether to keep decisions 19 and 34 and did not choose, so both were kept as they stood and are still provisional (decision 63). Remove this note once each is confirmed or changed.

| # | Decision | Outcome |
|---|---|---|
| 15 | First step above last step | **The range runs round through step 15 to step 0** (first 14, last 1 is four steps), so no pair of settings is invalid. |
| 16 | Pendulum | **Each end is played once per cycle** (0 1 2 3 2 1), not twice. Whether the A-154 does the same is not verified. |
| 17 | Addressing state | **A count of steps played in the cycle**, not a step number. Reset and end-of-cycle are then the same in every direction. A change of direction or range mid-cycle jumps to the matching point of the new cycle and does not turn round in place (ARCHITECTURE open item 7). |
| 18 | One-shot | **One cycle, then nothing begins; the clock keeps running.** A reset, or going back to looping, starts the pattern again on the beat. |
| 19 | Reset | **Deferred, as on the A-154**: the pattern goes back to its start and the first step plays at the next step boundary. The clock is not moved, and holding reset repeats the first step. Kept in phase 6 (decision 63): it is the run button that restarts the clock. |
| 20 | Scales and root | **Chromatic, major, natural minor, major and minor pentatonic**, root 0 to 11 semitones. A pitch is 0 to 45 semitones above the lowest. A scale is one table row to add. **No octave control yet**: its range depends on the DAC (decision 8). |
| 21 | Controls with no hardware | **Fixed in `app.c`**: all 16 steps, forward, looping, chromatic, root 0, never reset. They are ordinary core inputs, so giving them switches or pots later touches the app and an adapter, not the core. |
| 22 | Choosing between engines | **Not built yet.** `engine.h` sets the two-function pattern and the shared types; `seq.c` calls the plain engine directly. The selector comes with the second engine in phase 7, as a `switch`, not a table of function pointers. |
| 23 | The log line | **`Step 3 Note = 11` now gives the pitch in semitones** (the note value less one in the chromatic scale), and a rest is `Step 5 Rest`. |
| 24 | Scale tables | **In RAM as well as flash** (51 bytes). Flash only would need a compiler extension, which the core may not use. |

Taken while moving the shift registers to SPI. Also provisional, and to reconfirm with the ones above; 25 is the one to check first, on the board.

| # | Decision | Outcome |
|---|---|---|
| 25 | SPI mode | **Mode 0** (clock idle low, sample on the rising edge), MSB first. It samples on the edge the 74HC165 shifts on, which works because the chip's output changes a little after the edge, and it is the mode the MCP4822 and the 74HC595 also take, so the bus never has to change mode. Mode 2 would sample half a clock away from the shift and be safer for the 165 alone, at the price of switching modes for the other parts. **Not verified on hardware** (section 8). |
| 26 | SPI clock | **1 MHz** (system clock / 16). A panel read is 64 µs of clocks every 8 ms. 4 MHz is one bit in `spi.c` if the wiring proves clean; slower costs nothing that matters. |
| 27 | SS (PB2) | **An output held high from the start**, so the SPI cannot drop out of master mode. With phase 4 it is the DAC chip select: `spi.c` still sets it up, and `cv_output.c` drives it. |
| 28 | MOSI (PB3) | **An output, set by `spi.c`**, as of phase 4. It was left an input until something listened; the driver makes it an output, not the DAC adapter, because a bus pin belongs to the driver. |
| 29 | A failed panel read | **Leaves the caller's buffer untouched**: the adapter reads into its own 8 bytes and copies on success. Most of the 94 bytes the adapter grew by. |

Taken while building phase 3. Provisional like the ones above, and to reconfirm with them; 30 to 32 are the ones that are heard.

| # | Decision | Outcome |
|---|---|---|
| 30 | Gate length | **A number of clock pulses from the start of the step**, so it scales with the tempo. One setting for the whole pattern, read when a step begins. With no control for it, **fixed at 12 (half a step)** in `app.c`. |
| 31 | Ties | **A length of a whole step (24) or more ties consecutive notes** into one gate; a rest ends it. It falls out of the arithmetic, with no tie flag. A tie or a length for one step alone comes with the engines of phase 7 (the M185-style gate modes), through `engine_step_t`. |
| 32 | Clock output | **One pulse per step, high for the first half** (12 pulses), rising as the step begins. It follows the clock, not the pattern: it runs through rests and after a one-shot has finished. A step being a sixteenth note, that is 4 pulses per quarter note. |
| 33 | A step that cannot be read | **No gate.** The step still takes its turn and its clock pulse. |
| 34 | Reset and a held note | **A reset does not cut the gate short**; the note in progress plays out. Kept in phase 6 (decision 63); stopping with the run button does cut it. |
| 35 | A step begun late | **Its gate is counted from when the step was due**, so the note still ends on time, and does not open at all if that time has passed. The clock output stays low while steps are owed. |
| 36 | Applying the outputs | **Both pins are written on every tick, before the log**: gate, then clock. Active high, low from reset. If the buffer stage inverts, the inversion goes in the two pin adapters. |
| 37 | Ports | **Two, `gate_output_port` and `clock_output_port`**, one function each, on the pattern of the input ports, in place of the single `gate_port` section 4 names. The pins are set up in `register_init.c`. |

Taken while building phase 4. Provisional like the ones above, and to reconfirm with them. 40 and 42 are the ones to check on the board.

| # | Decision | Outcome |
|---|---|---|
| 38 | DAC setup | **Channel A, gain 2, internal reference**: 1 mV a step, 0 to 4.095 V. The 0 to 45 semitones of decision 20 are 0 to 3.75 V at 1 V to the octave, so the buffer needs no gain. Channel B is not used. LDAC is tied low, so the output changes as chip select rises. |
| 39 | The port | **`cv_output_port`, one function, in nominal millivolts** (0 to 4095), on the pattern of decision 37, in place of the `cv_port` section 4 named. Millivolts and not a DAC code, so that another DAC is only another adapter. |
| 40 | Lowest pitch | **0 V.** Note value 1 with a root of 0 is 0 V and each semitone is 83.3 mV above the last. Where that sits musically is the oscillator's tuning knob. A DAC cannot quite reach 0 V (its output stops a few millivolts short), so the lowest note may read slightly sharp; a small offset in the table would cure it, at the price of the same off the top. To look at with a meter. |
| 41 | Calibration table | **Five entries, one per octave, with straight lines between**, rounded to the nearest step. The ideal table (0, 1000, 2000, 3000, 4000) is what `app.c` uses until a measured one is stored (phase 5, decisions 47 to 54). The table may fall as well as rise, so an inverting buffer is only a different table. |
| 42 | When the DAC is written | **On every tick, changed or not**, ahead of the gate. The apply step then takes the same time every tick, so the gate and clock edges do not move when a note starts, and a failed write is retried within a millisecond. The cost is 20 µs of bus activity a millisecond beside an analogue output. If that can be heard, write only on a change (a few lines in `cv_output.c`) and accept gate edges about 22 µs later on note ticks than on others. |
| 43 | Pitch and gate | **In the same tick, pitch first.** The DAC takes the pitch 1.6 µs before the gate pin rises, which is less than its settling time, so the gate leads the settled pitch by a few microseconds. Holding the gate back a tick in the core is the fix if it is ever wanted. |
| 44 | Rests | **The pitch holds.** Only a note changes it: not a rest, an unreadable step, a reset or the end of a one-shot pattern, so a sound still dying away keeps its pitch. 0 V until the first note. |
| 45 | A failed write | **Logged once per step, after the note**, like a failed read. The note still plays, at the old pitch; the next tick writes again. |
| 46 | The chain during a DAC write | **Left to shift.** The 74HC165s have no chip select and are clocked by every DAC frame. They are loaded afresh before each read, so it does no harm. |

Taken while building phase 5. The first was the owner's choice on 2026-10-10, between that, switches alone, and leaving the mode until phase 6 has a button. The rest are provisional like the ones above, and to reconfirm with them; 48 to 52 are the ones met at the panel, so go through them with the board and a meter in hand.

| # | Decision | Outcome |
|---|---|---|
| 47 | How calibration is operated | **The panel switches and the tempo pot, with no new hardware.** It works the same in both builds. |
| 48 | Entering calibration mode | **A reserved panel pattern, looked at once, at power-on**: the last four steps reading 10, 5, 10, 5 (their switches alternating like a chequerboard). Any other panel plays as normal, and so does one that cannot be read on the first tick. The mode lasts until the power is cycled. Setting the pattern while playing does nothing. |
| 49 | Choosing the point | **The first step's switches, as a number 0 to 4**: point n is the voltage for n octaves above the lowest pitch. A higher number is point 4. |
| 50 | Trimming | **The tempo pot adds −64 to +63 mV to the point's nominal value** (n × 1000 mV), one millivolt for every 8 counts of the reading, with the middle of the pot at nominal. There is no hysteresis, so the value can flicker by one step at a boundary. The range covers a gain error of 1.6 % at 4 V; a part that is further out needs a wider range, which is two constants in `cal.c`. |
| 51 | Storing | **Switching on the lowest switch of the last step** stores the value being given as the point chosen. It is on when the mode is entered (the pattern needs it), so it is switched off and on again. Only that point changes; the others keep what was loaded. The procedure is "turn the pot until the meter reads n.000 V, then flip the switch", so nothing has to be worked out or typed in. |
| 52 | Showing that it happened | **The gate is high in the mode and goes low for 250 ms after a store**; the clock output stays low. The debug build also logs `Cal point 2 = 2013`. |
| 53 | The record | **12 bytes at EEPROM address 0**: a version byte (1), the five entries as 16-bit values with the low byte first, and a CRC-8 (polynomial 0x07) of those. Erased storage, a wrong version and any single wrong bit are all refused. **One copy**: if the power goes during the 40 ms a full write takes, the checksum fails at the next start and the ideal table is used, so the whole calibration is lost and not one point. A second copy written after the first would cure that. |
| 54 | Loading | **Once, at start-up**, over the ideal table. Anything that is not a valid record leaves the ideal table and is not an error; a read that fails is logged. |
| 55 | The storage port | **`nv_port`: a read that returns at once, a write that is only started, and a poll.** The write copies up to 16 bytes; `nv_poll()` deals with one a pass, and skips a byte that already holds the value, so storing one point writes two or three bytes, not twelve. A second write while one has bytes to go is refused with `ERR_BUSY`. Nothing reports completion, because nothing needs it. |
| 56 | Interrupts during a write | **Held off for the two instructions that start each byte**, since the second must follow the first within four clock cycles. That can delay one tick interrupt by under a microsecond. |
| 57 | Reset cause | **Read and cleared once at start-up, and logged**, as a number with a bit for each cause (1 power-on, 2 reset pin, 4 brown-out, 8 watchdog). Nothing acts on it until the crash log of phase 9. |
| 58 | The start-up log line | **`Reset = 2 Cal = 1`, kept to 19 characters** so that it has gone before the first step's line is queued. It still pushes that first line back from 2.3 ms to 2.6 ms after reset. |
| 59 | Where the mode sits in the core | **A module of its own, `cal`, beside `seq`.** The app calls `cal_tick()` first and `seq_tick()` only when it says the mode is not active, so the sequencer is untouched. The core now has two functions the app calls each tick, not one. |

Taken while building phase 6. The first two were the owner's on 2026-10-10. 62 and 63 were put to the owner with a recommendation and not answered, so the recommendation was built; go through those two first. The rest are provisional like the ones above. 64 to 67 and 72 are the ones to check with a real clock patched in.

| # | Decision | Outcome |
|---|---|---|
| 60 | Choosing the clock | **A sync switch**, the owner's idea, chosen over the first edge taking over (with or without a timeout) and over a switched jack. Off: the internal clock, with the clock input ignored even if something is plugged in. On: the external clock, from the first tick; with no edges, nothing plays. The owner may take the switch out again later. Without it the choice would be made by the edges themselves, which is a change inside `transport.c`. |
| 61 | The tempo pot on the external clock | **A ratio to that clock**, so that the pot is never idle: its range in seven parts gives a step to every 4, 3 or 2 edges, a step an edge in the middle, and 2, 3 or 4 steps an edge. Gate length and swing were the other uses considered; a ratio keeps the knob a speed control in both positions of the switch. No hysteresis at the six boundaries. |
| 62 | The run button | **A toggle. The board powers on running.** Stop cuts the gate at once and holds the clock output low; the pitch CV holds. Start goes back to the first step, which begins at once on the internal clock, with the clock restarted so that the step is a whole one. The alternative was a pause that carries on where it stopped. |
| 63 | The reset input | **Deferred, as decisions 19 and 34 had it**: the first step plays at the next step boundary, which on the external clock is the next sync edge, the clock is not moved, and a held note plays out. Ignored while stopped. The alternative was an immediate restart on the internal clock. |
| 64 | An external edge | **One edge is one step at a ratio of one**, a sixteenth note. Every divide-th edge is a sync edge, and exactly 24 × multiply pulses are given from one to the next, spaced by the time between the last two. A late edge is waited for one pulse short, so no step begins ahead of its edge. An early one gets all that is left at once, and any steps that were still owed begin on the ticks that follow: none is dropped, so the pattern cannot slip against the clock. An edge that comes in under half the last gap finds the gate and the clock output still high from the step before: its step is held back one tick, with both low, so that it has a rising edge of its own on each. The owner chose that on 2026-10-10 over leaving the two steps run together; the cost is that such a step is a millisecond later than the rest. |
| 65 | A step with no spacing to go by | **Taken to be 20 ms long, with the multiply as one**: the first after the clock starts, and the first after a gap of 4 s or more. Its gate and clock pulse are 10 ms whatever the tempo. The 20 and the 4000 are two constants in `clock.c`. |
| 66 | A change of clock | **Does not restart the pattern.** Going to the external clock, the step in progress is wound up within 20 ms and the next waits for an edge. Going back, the internal clock carries on from there. |
| 67 | How an edge is seen | **A flag the MCU latches, polled once a tick, with its interrupt off**: Timer/Counter1's capture flag for the clock, with the noise canceller on, and external interrupt 0's flag for reset. So a pulse of any width is caught, a step begins 1 to 2 ms after its edge, and spacing is known to the millisecond. The capture's own timestamp is not read; phase 11 could. **Not verified on hardware** (section 8). |
| 68 | The input stage | **Assumed to invert**: a transistor pulling the pin low while the jack is high, with the MCU's own pull-up. The firmware therefore looks for a falling edge at the pin. Not designed in detail or built; if it does not invert, it is one bit in each of the two init functions. The sync switch and the run button close to ground, also on the internal pull-ups. |
| 69 | Debounce | **20 ms, in the core**, for the button and the switch alike, from readings taken every tick. The first reading is taken as it stands, so a switch on at power-on is on from the first tick, and a button held down at power-on is not a press. |
| 70 | Ports | **Four, one function each**: `clock_input_port` and `reset_input_port` report and forget an edge, `run_input_port` and `sync_input_port` give a level as read. They are read on every tick, not on the 8 ms scan. |
| 71 | The first step | **A whole step**: the tick that starts the pattern does not count towards its second step, after reset and after the run button alike. This closes decision 14 and ARCHITECTURE open item 4. |
| 72 | Pins | **PB0 clock in, PD2 reset in, PD3 the sync switch, PD7 the run button**, as section 7 proposed, but for PD3, which was to be a run input or a spare. PD6 is the one free pin on port D. |
| 73 | The log line | **`Transport = 2`**, sent on a change: 0 stopped, 1 the internal clock, 2 the external. Nothing is sent at power-on unless the switch is on. |

## 3. What the survey found

Full feature matrix and sources are in the research notes summarised here. Bindubba is a Nonlinearcircuits 4x4 sequencer addressed by two independent clocks, not a touch-keyboard panel.

| Design | Idea worth taking | Fits the 64 switches as |
|---|---|---|
| Klee | A shift register holding many active bits; the output is the sum of the values under every active bit. Three gate buses, bus 2 being "neither 1 nor 3". Merge turns consecutive active steps into one long gate. Invert-on-recirculate doubles the loop | 16 nibble values summed under a 16-bit register in RAM (maximum 240, fits `uint8_t`). The switches are the preset; "load" is a core event |
| Bindubba, René | 16 values addressed as (x, y) by two clocks at different rates | Two internal dividers over the same 16 steps. No hardware |
| 0-CTRL | Per-step time and strength; step gates patched back into reset, stop and direction | Row B nibble as per-step time or strength; a per-step action table |
| RYK M185, Metropolis | Per-step pulse count and gate mode (hold, repeat, single, rest); fixed total length; ratchets; slide | Steps 1 to 8 as pitch, steps 9 to 16 as 2 bits of gate mode plus 2 bits of pulse count. Fits exactly |
| Doepfer A-154 | Direction modes, first and last step, one-shot, deferred reset | A "next address" function in the core |
| Turing Machine | Recirculating bit through a probability gate; lockable loop | Shares the Klee register plus one LFSR |
| Korg SQ-1 | One mode knob reinterprets the second row | The engine selector itself |
| Grids, Euclidean | Fill count spread evenly over a length | A nibble as the fill count |
| ARP 1601 | A gate bus routed to skip or reset | Reserved note values |

Out of scope for this hardware: touch plates, pressure, analog envelopes.

The design consequence: **an engine is a pure function from the panel bits, the engine's own state and the clock to a note, a gate and a next address.** Adding one is a core file and its tests.

## 4. Architecture as it scales

### Layers

The ports-and-adapters rules stay as they are. Three additions:

- **Core becomes several modules**, each one purpose, each with its own test program:

  | Module | Purpose |
  |---|---|
  | `clock` | Elapsed ticks to musical pulses (internal tempo or external edges). Both built; decisions 64 and 65 |
  | `transport` | Stopped, running, external sync, as an explicit state machine (built; decisions 60 and 62) |
  | `address` | Next step: direction, first and last step, one-shot, reset |
  | `engine_*` | One per engine: plain, 8x2, Klee. Same interface |
  | `note_map` | Scale degree, root and octave to a semitone |
  | `pitch_cal` | Semitone to DAC code through a calibration table |
  | `cal` | Calibration mode: the panel and the pot used to set that table (built; decisions 47 to 52) |
  | `cal_record` | The table's stored form, with a version and a checksum (built; decision 53) |
  | `gate` | Gate length, ties, ratchets, in pulses |
  | `debounce` | Buttons and switches (built; decision 69) |
  | `prng` | One LFSR shared by every random feature |
  | `errlog` | Crash record format |

- **Ports are named for what the core needs**, not for the bus: `timebase_port`, `gate_output_port` and `clock_output_port` (built; decision 37), `cv_output_port` (built; decision 39), `nv_port` and `reset_cause_port` (built; decisions 55 and 57), `clock_input_port`, `reset_input_port`, `run_input_port` and `sync_input_port` (built; decision 70), `led_port`, `watchdog_port`. No `spi_port`.
- **`adapters/target/` gets two levels**: peripheral drivers (SPI, Timer2, EEPROM) with headers private to that folder, and device adapters above them that implement ports (the DAC adapter implements `cv_port`).

### The tick

- A 1 kHz Timer2 compare interrupt increments one file-static `volatile uint8_t`. That is the whole ISR.
- `timebase_port` returns ticks elapsed since the last call. A one-byte read is atomic on AVR, so no masking is needed, and an overrunning loop loses no ticks.
- The core receives `elapsed_ticks` as an ordinary input. The host fake returns scripted values.
- Internal resolution is 96 pulses per quarter note (24 per step). An integer accumulator converts ticks to pulses with no drift: each tick adds `bpm * 8`, and every 5000 emits one pulse. It fits `uint16_t`. Never round the pulse period to whole ticks; at 300 BPM that is a 4 % tempo error.
- Outputs are applied at the tick boundary: each pass first applies what the previous pass computed, then gathers inputs and runs the core. Every output is one tick late, by the same amount every time.
- The loop never blocks. `delay_port` goes away. The serial log becomes non-blocking (one byte per pass when the transmitter is ready), or debug and release will keep timing differently.

All of the above is built as of phase 1; ARCHITECTURE.md describes it as it stands.
- Switches and pots are scanned at 100 to 200 Hz, not every tick.

Register values for the tick (Timer2, CTC, /64, compare 249 gives exactly 1000 Hz) and the vector symbol are in section 8 with their verification status.

### Rules to add as the code grows

- Each ISR-shared variable is a file-static `volatile` in exactly one adapter and is reached only through a port function.
- Registers are grouped one header per peripheral, as the USART already is, so a peripheral used by one configuration does not become a rule 2.5 finding in the other.
- The ISR attribute comes from a macro in the register header (the `REG_IO_LOW` pattern), so a host test can include the adapter and call the ISR directly. Coverage stays at 100 %.
- A third build configuration (`CONFIG=midi`) is another link-time choice, not a macro.

The first ISR brought one cppcheck finding, and not either of the two predicted (the reserved vector name and the attribute went unflagged): rule 8.7, advisory. It is a false positive: the handler is referenced from the vector table in the startup object, which the tool does not see, so it looks unused outside its file. It is suppressed inline with the reason. Expect the same for each further ISR.

## 5. Emulation

### What was proven tonight

The unmodified `build/debug/output.hex` was run under avr8js 0.21.1 on Node 22 with a modelled 74HC165 chain, tempo pot and UART capture. Nothing was installed system-wide and nothing was added to the repo.

| Check | Result |
|---|---|
| Log lines against the panel pattern set in the model | All matched, over 1249 consecutive steps |
| Shift register pulses per tick | 1 load, 64 clocks |
| Tick cost besides the tempo delay (debug) | 1.5 to 1.7 ms, longer for two-digit steps and notes |
| Step period, pot at 0 V / 0.5 V / 5 V | about 1.6 ms / 26.5 ms / 256.7 ms |
| Release image | Runs; USART never enabled |
| Speed | 1 simulated second in about 0.2 s |

These agree with the calculated figures in ARCHITECTURE.md, and are the first measurement of them.

One lesson came with it. The first shift register model latched on the falling edge of the load line and reported the wrong note on the very first tick. The firmware was right; the real chip loads for as long as the line is low. **A part model encodes one reading of the datasheet, so the board stays the final check.**

### Other tools considered

| Tool | Why not the base |
|---|---|
| simavr | Strong candidate: complete 328P peripherals, VCD, gdb, actively maintained. Upstream targets Linux and macOS; a Windows build is community-proven but untested here. Worth a one-hour spike if gdb against the emulator becomes important |
| QEMU AVR | Models only the USART and 16-bit timer for this chip. No GPIO or ADC. Unusable |
| Wokwi CLI | Has the 74HC165 and runs prebuilt images, but simulation runs in their cloud, needs a token, and the free tier is 50 minutes a month. No pulse-count assertions |
| Renode | Excellent for Cortex-M and RISC-V later. No AVR |
| SimulIDE, PICSimLab, Proteus | GUI-first; not scriptable from make |
| avr-tester (Rust) | The closest existing test-first AVR harness. Linux and macOS only |

### The harness (`tools/sim/`, `make sim`)

In place as of 2026-10-10: `lib/machine.js` (the chip), `lib/pin_recorder.js`, `lib/edge_flag.js`, `parts/hc165.js`, `parts/mcp4822.js`, `parts/clock_source.js`, `parts/switch.js`, `boards/nano_sequencer.js` (wiring, panel, pot, the DAC's frames, gate and clock recorders, the two input jacks, the sync switch and run button, serial capture, the EEPROM's contents) and seventy-two scenarios across the two images. ARCHITECTURE.md describes it.

Still to add, each when a phase needs it:

- **Timers and interrupts in `lib/machine.js`**: Timer/Counter2 added in phase 1, the SPI with the shift register move, the EEPROM in phase 5, Timer/Counter1 in phase 6. Phase 6 also needed something avr8js does not have: a flag that latches an edge with its interrupt off, for the input capture and for an external interrupt. `lib/edge_flag.js` supplies it, written from the datasheet, so it cannot settle whether a real chip behaves that way (section 8).
- **More parts**: 74HC595 (phase 8). The MCP4822 was added in phase 4, and the clock source and the switch (which is also the button) in phase 6. SPI parts hook the byte transfer, not the pins: the board hands every transfer to every part, and a part with a chip select takes it only while selected.
- **A pin recorder**: added in phase 3, as a list of edges and of completed pulses per pin; the scenarios do their own arithmetic on them. Helpers such as `count_rises(pin, t0, t1)` can be added when a scenario wants one. The 74HC165 model still counts its own pulses.
- **VCD dump on failure**, viewable in GTKWave or PulseView.
- **A board file as plain data**, once there is a second board.

What it cannot show: analog behaviour (DAC accuracy, settling, tuning), switch bounce unless scripted, and anything where the emulator's timer or ADC model differs from silicon.

It is separate from `make coverage`, which stays a host-build measurement.

## 6. Claude Code context cost

Verified against the current Claude Code documentation:

- Root `CLAUDE.md` loads in full every session. `@` imports are also loaded eagerly, so they organise but do not save.
- `.claude/rules/*.md` with a `paths:` list, and `CLAUDE.md` files in subfolders, load only when a matching file is read or edited.
- A skill costs one description line until it is invoked. With `disable-model-invocation: true` it costs nothing until you type its name.
- A subagent has its own context and returns only a summary, so it keeps noisy output out of the main session. It also starts cold and spends its own tokens. Tonight's four research agents used about 554,000 tokens between them; that is the expensive path and is worth it for research, not for routine work.
- Block-level HTML comments in CLAUDE.md are stripped before loading, so maintainer notes there are free.

### The split

Done on 2026-10-10. The root `CLAUDE.md` went from about 2,500 words (5.1k tokens by `/context`) to about 880 words. Check the new figure with `/context` in a fresh session.

Nested `CLAUDE.md` files load only when a file in their folder is read or edited, provided Claude is started from the repo root. The root file lists them and says to read a folder's file before adding to that folder.

| Where | Content |
|---|---|
| Root `CLAUDE.md` | Architecture rules, the one-line coding standard, the list of folder files, the checks that define done, the always-on MISRA rules, working style, project facts |
| `core/CLAUDE.md` | What keeps the core pure |
| `ports/CLAUDE.md` | Port naming, header documentation, `port_status_t` numbering |
| `adapters/target/CLAUDE.md` | Registers in two files, `REG_IO_LOW`, builtins, the two loggers, UART and pin cautions |
| `adapters/host/CLAUDE.md` | The PC fakes |
| `app/CLAUDE.md` | The tick order and `main.c` |
| `tests/CLAUDE.md` | Which kind of test, the fake register header recipe, coverage |
| `tools/misra/CLAUDE.md` | How the analysis runs and how findings are handled |
| `tools/sim/CLAUDE.md` | Part and scenario conventions |
| Removed | Size figures, test counts, the description of what the firmware does today and the open items list. ARCHITECTURE.md owns them |

Still to do, as later phases add them:

- The driver and device layers in `adapters/target/CLAUDE.md`: done with the SPI move. The ISR and `volatile` rules went in with phase 1.
- Module and engine conventions in `core/CLAUDE.md`: done with phase 2.
- Skills for the multi-step recipes (`add-register`, `host-test-adapter`, `new-engine`), if the folder files grow too long.

### Other measures

- **Keep build output out of context.** Deny reads of `build/**`, `*.hex`, `*.o` and `*.gcov` in `.claude/settings.json`.
- **Quiet check targets.** A `make check` that runs the build, tests, coverage and MISRA and prints one line per stage unless something fails saves more than any instruction change, and helps you at the terminal too.
- **A `firmware-checker` subagent on a small model** for the full check run, returning only failures. Not worth it for a single `make`.
- **Deny `make flash`** in settings, which turns a written rule into an enforced one.
- **`/clear` between unrelated tasks**; the code and ARCHITECTURE.md carry the state.

One caution from the documentation: whether a read deny also blocks `cat` through the shell is not confirmed. Check with `/context` after the change.

## 7. Phases

Each phase is one or more small commits, leaves both builds, the tests, coverage and MISRA green, and updates README and ARCHITECTURE in the same piece of work. "Sim" means a scenario under `make sim`. "Board" means it needs you to flash and report.

| Phase | Adds | New MCU peripherals | New parts | Verified by |
|---|---|---|---|---|
| 0. Groundwork | `make sim` with the first three parts (**done**); the CLAUDE.md split (**done**); a quiet `make check` (**done**); Optiboot on the board (any time before phase 9, easiest before phase 4) | None | ISP programmer | Sim scenarios pass; the board still uploads and runs |
| 1. Timebase (**done**, not yet run on the board) | Timer2 tick, `timebase_port`, core `clock`, tempo in BPM with a floor, non-blocking loop and log. Removes `delay_port`. Closes open items 1 and 3 | Timer2, first ISR | None | Host: exact pulse counts over N ticks. Sim: step period matches BPM in the debug build; the release build has no output to time a step by until phase 3, so its scenarios check the 1 kHz tick instead |
| 2. Addressing and notes (**done**, not yet run on the board) | `address` (direction, first and last step, one-shot, reset), `note_map` (scales, root, rest). Engine interface, with the plain engine as its first user. Also `panel`, the switch layout every engine shares | None | None | Host: every module on its own and under `seq`. Sim through the log: pitches and rests only, because the board has no controls for the rest (decision 21) |
| 3. Gate and clock out (firmware **done**, not yet run on the board; the buffer stage is still to build) | Core `gate` (length in 1/24 step, ties), `gate_output_port`, `clock_output_port` | GPIO PD4, PD5 | Buffer IC, resistors, jacks | Sim: pulse widths and counts. Board: logic analyser or LED |
| 4. SPI and pitch CV (firmware **done**, not yet run on the board; the DAC is still to buy and wire, and the buffer to build) | SPI driver and the 74HC165 chain on SPI (ahead of phase 3; the board needs rewiring first); `cv_output_port` and its MCP4822 adapter; `pitch_cal` with the ideal table. First point it plays an oscillator | SPI | MCP4822, op-amp buffer, 1 kΩ on MISO | Sim: exact DAC frames, DAC written before gate rises. Board: tuning by ear and meter |
| 5. Calibration and storage (firmware **done**, not yet run on the board, which has no DAC yet) | Calibration mode on the panel switches and the tempo pot (core `cal`), `nv_port` and its non-blocking EEPROM adapter, the record with version and checksum (core `cal_record`), `reset_cause_port` | EEPROM | Multimeter | Host: record format, the mode, the adapter. Sim: EEPROM contents, and a calibration stored on one power-up played on the next. Board: measured octaves |
| 6. Clock, reset and run in (firmware **done**, not yet run on the board; the input stages are still to design and build, and the button, switch and jacks to wire) | One external edge per step with a ratio set by the pot, reset, a run button and a sync switch, `transport`, `debounce` | Timer1 capture flag and INT0 flag, polled | Two transistor input stages, button, switch, jacks | Host: the clock, the transport, the debounce and the four adapters. Sim: a scripted clock source, reset pulses, a bouncing button. Board |
| 7. Engines | 8x2 row modes (per-step time, then pulse count and gate mode), Klee (register, sums, buses, merge, invert, load), `prng` with Turing lock and gate probability, Cartesian and Euclidean | More ADC channels | Ninth 74HC165, pots | Host, mostly. Sim for gate timing |
| 8. Step LEDs | 16 LEDs in the same SPI frame as the switch read | None | Two 74HC595s, LEDs | Sim: 595 model. Board |
| 9. Watchdog and crash log | Watchdog kick, `errlog` ring in EEPROM, `.noinit` breadcrumb. Needs decision 3 | Watchdog | None | Sim: watchdog reset and recovery. Board |
| 10. MIDI | `CONFIG=midi`: clock and notes out, then in | USART at 31250 baud (exact), RX interrupt | DIN sockets, optocoupler | Host: parser and generator. Sim: UART bytes |
| 11. Later | 24 PPQN sync, and timing an edge more finely than a tick (the fitting of steps between edges was built in phase 6); second DAC channel, slide, swing and ratchets if not already in | Timer1 timestamps | — | — |

Notes on ordering:

- Phases 2 and 7 are pure core work and can be done at any time, with no hardware. Phase 7 is listed late only because gates and pitch make it audible.
- The shift register move changed the hex, so the `cmp` proof did not apply; the sim scenarios were the regression check, and they cannot check the SPI mode (decision 25).
- Phase 1 is the largest architectural change (first interrupt, no more blocking). It is first because gate length, ratchets, swing and external clock all depend on it.

### Proposed pin assignment (from phase 4)

In use today: PB0 to PB5 as below (the 74HC595 not yet), PD2 to PD5, PD7, PD1 in the debug build, and ADC6. The rest is still a proposal.

| Pin | Use | Pin | Use |
|---|---|---|---|
| PD0, PD1 | UART (log or MIDI) | PB0 (ICP1) | Clock in |
| PD2 (INT0) | Reset in | PB1 | 74HC165 load and 74HC595 latch |
| PD3 (INT1) | Sync switch (decision 72) | PB2 (SS) | DAC chip select |
| PD4 | Gate out | PB3 (MOSI) | DAC and 595 data |
| PD5 | Clock out | PB4 (MISO) | 74HC165 data, through 1 kΩ |
| PD6 | Accent or second gate | PB5 (SCK) | SPI clock (on-board LED flickers) |
| PD7 | Run/stop button | PC0 to PC3 | CV in, pots |
| ADC6 | Tempo pot (unchanged) | PC4, PC5 | Reserved for I2C |
| ADC7 | Pot or CV in | | |

Where the 328P runs out, in order: one USART (log or MIDI, not both), no on-chip debug, a 10-bit ADC (fine for quantised transpose, not for pitch tracking), three timers. The smallest step up is the ATmega328PB (two USARTs, same register model); any move costs a new adapter folder and leaves the core and ports alone.

## 8. Not verified

Check these before the phase that depends on them.

- **Register bit positions** inside the watchdog control register, and the OCR1A address. Addresses, prescaler codes and vector numbers were read from the datasheet; bit positions were not. (Phase 9) The EEPROM ones in use (EERE, EEPE, EEMPE, and the four addresses) and the address of MCUSR with its four reset flags are now **verified** against the toolchain's `iom328p.h`, and the EEPROM ones by the emulator storing and reading back a record through them. The SPI ones in use (SPE, MSTR, SPR0, SPIF, and the three addresses) are now **verified** against the toolchain's `iom328p.h` and by the emulator running transfers at 1 MHz from them. The Timer2 ones used in phase 1 are now **verified** against the toolchain's own `iom328p.h` and by the emulator producing a 1 kHz tick from them. That header ships with the toolchain and is a quick check for the rest.
- **`__vector_7` as the Timer2 compare A symbol**: now **verified**. It links, the vector table entry jumps to the handler, and the handler runs on the emulator.
- **Edge flags latching with their interrupts disabled** (for the polled clock and reset inputs). Still not verified, and now built on: the firmware of phase 6 polls ICF1 and INTF0 with their interrupts off. The datasheet's wording says each flag is set by the edge and the interrupt taken only if it is enabled, which is how these parts are generally understood to behave, but that is from memory and has not been tried. The emulator is no evidence here, because avr8js does not model it and the model in `lib/edge_flag.js` was written for this project. Also from memory: that the input capture works with nothing more set up than `clock_input_init()` does (the timer is left running to be safe), and the bit positions and addresses used, though those were checked against the toolchain's `iom328p.h` (ICNC1, ICES1, CS11, ICF1, ISC01, INTF0; TCCR1A, TCCR1B, TIMSK1, TIFR1, EICRA, EIMSK, EIFR). See ARCHITECTURE.md open item 12 for the order to check things in. (Phase 6)
- **The input stage for the two jacks**: not designed. The firmware assumes it inverts. The MCU's pull-up is 20 to 50 kΩ from memory of the datasheet. (Phase 6)
- **The clock, reset, run button and sync switch on the board**: nothing has been tried. The 20 ms debounce, the seven-part ratio with no hysteresis, the 1 to 2 ms from an edge to its step, and reset arriving with or just after its clock edge are the things to look at. (Phase 6)
- **SPI mode for the 74HC165 alongside the DAC**, and one pin serving as both 165 load and 595 latch. Bench-check. (Phases 4, 8) The firmware now uses mode 0 for both the 165 and the DAC (decision 25); the first board run after rewiring is that bench check. Read a panel with a single switch on and confirm it lands on the right step and bit.
- **The MCP4822 details the DAC adapter is built on**: the frame layout (bit 15 channel, bit 13 gain with 0 meaning 2x, bit 12 output enable with 1 meaning on, then 12 data bits), the PDIP-8 pinout in ARCHITECTURE.md, how a frame of other than 16 clocks is treated, the 4.5 µs settling time, and what the output does between power-on and the first write. Written from memory of DS20002249B, and now **verified** against it, with two things to add. A frame cut short of 16 clocks is ignored, as assumed, but one that runs over is not: the first 16 are taken and the rest ignored (the firmware never sends one; the emulated part now does the same). And from power-on until the first write the output is off, at about 500 kΩ, not at 0 V. The same reading confirmed that the part takes SPI mode 0, clocking data in on the rising edge, up to 20 MHz. The 74HC165's DIP-16 pin numbers in `docs/SCHEMATIC.html` are **verified** against TI's SCLS116H, which also gives a minimum load pulse of 16 ns at 4.5 V, against the 125 ns sent. A meter on the output is still the check of the gain and enable bits on the board. (Phase 4)
- **The pitch CV on the board**: nothing has been measured. See ARCHITECTURE.md open item 10 for the order to check things in. (Phase 4)
- **Calibration on the board**: nothing has been tried, and it needs the DAC first. See ARCHITECTURE.md open item 11. Three things only the board can show: that the pot is steady enough to hold a value to a millivolt while the switch is flipped (decision 50), that ±64 mV is enough range for the real part and buffer, and that a bouncing store switch does no harm (it should not: the switch is sampled every 8 ms, and a repeat store either writes the same bytes, which are skipped, or is refused while the first is still going and logged as a failed write). (Phase 5)
- **How long the EEPROM takes.** The 3.4 ms a byte is the datasheet's typical figure, from memory; the emulator models 3.6 ms. Nothing depends on it, since the firmware waits for the hardware's own flag. (Phase 5)
- **What the reset cause reads on this board.** The old bootloader runs before the firmware and may or may not leave MCUSR alone, and Optiboot, from memory, clears it and passes the value on in a register instead (the fuse and bootloader item below). On the emulator the register simply holds what the scenario puts there. Read the first log line after a power-up and after pressing reset; if both say 0, the bootloader is taking it. (Phases 5, 9)
- **The gate and clock outputs on the board.** Their timing is measured on the emulated board only, and as logic levels: check one period and one width on D4 and D5 with a logic analyser. The output stage (buffer, series resistors, levels at the jack) is not designed in detail or built. (Phase 3)
- **Fuse values, the Nano's brown-out level, and how Optiboot passes on the reset cause.** Check against the Arduino board definitions before burning anything. (Phase 9)
- **avr8js accuracy for timers and interrupts.** Phase 1 runs on its Timer2 model and gives an 8.000 ms scan period and a 100 ms step at 150 BPM. Still to do: compare one of those against the board.
- **MIDI electrical values**, and that the opto must be disconnected to upload. (Phase 10)
- **Survey items read only from retailer or secondary pages**: Moog 960, ARP 1601, Buchla 245/246/248, Serge, René, Turing Machine details. The Klee, Bindubba, 0-CTRL, M185 and A-154 descriptions are from their manuals.
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
