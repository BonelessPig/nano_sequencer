# nano_sequencer decisions

The decisions taken while planning and building the firmware, moved here from [ROADMAP.md](ROADMAP.md) on 2026-10-11.
The roadmap carries the plan, and its "Next" section says which decisions the next piece of work depends on; a "section N" or "phase N" below is the roadmap's, and an "open item" is one of OPEN_ITEMS.md's.

Decision numbers are referred to from the code's docs; never renumber them.
Unless a group says otherwise, its decisions were left to recommendation, are **provisional**, and have not been heard or run on hardware.

**The owner's, 2026-10-10.**

| # | Decision | Outcome |
|---|---|---|
| 1 | Emulator base | **avr8js**, as `make sim`. simavr may be added later for gdb and VCD; its Windows build is untested. |
| 2 | Harness location | **In this repo** (`tools/sim/`), with `lib/` and `parts/` free of anything sequencer-specific so they can be extracted at the second consumer. |
| 3 | Bootloader | **Optiboot, burned by ISP**; the final board may have none. The stock old bootloader never disables the watchdog (verified in its source). The Makefile's upload speed then goes from 57600 to 115200. Not yet burned (decision 106). |
| 4 | Shift registers | **The 74HC165 chain is on hardware SPI.** Frees INT0 and INT1 and shares the bus with the DAC and the LED chain. Three signals to rewire: load D2 to D9, clock D3 to D13, data D4 to D12 through 1 kΩ. |
| 5 | Output timing | **Outputs are applied at the tick boundary**: a constant 1 ms latency for near-zero jitter. |
| 6 | Rests | **Note 0 is a rest**, leaving 15 pitches. |
| 7 | Choosing a mode | **A ninth 74HC165** (8 more switches, no MCU pins). The raw input is 9 bytes (decisions 74 and 75). |
| 8 | Pitch DAC | **MCP4822** (12-bit, SPI, internal 2.048 V reference). The MCP4725, PWM and R-2R all take their reference from the USB 5 V rail, where a 1 % supply change is about 48 cents at 4 V. A 16-bit part (DAC8562/8563, AD5683R/AD5693R) is the upgrade if steps finer than 1.2 cents are ever wanted: a new adapter behind the same port. |
| 9 | Instructions for Claude | **Nested `CLAUDE.md` files per folder.** The C standard and the line length rule are also in the user-level file. `core/` and `adapters/target/` are flat folders, so the lessons about one part of either are in `.claude/rules/core-*.md` and `target-*.md`, each with a `paths:` list, and load only with the files they are about. |

**Phase 1.** Each is one constant or a few lines to change.

| # | Decision | Outcome |
|---|---|---|
| 10 | Tempo range | **30 to 285 BPM, linear**: 30 plus the pot reading divided by 4. A step is a sixteenth note (24 pulses). |
| 11 | Input scan rate | **Every 8 ticks (125 Hz)**, panel and pot together, scheduled by the app. |
| 12 | Log when the queue is full | **Drop the whole message.** The queue is 128 bytes. Failed reads are logged once per step, not once per scan. |
| 13 | Steps owed after a stall | **Begin one per tick until caught up**, none skipped. |
| 14 | Start-up | **The first tick begins step 0**, and it is a whole step (decision 71). |

**Phase 2.** Decisions 15 to 24 were confirmed by the owner as built, on 2026-10-10.

| # | Decision | Outcome |
|---|---|---|
| 15 | First step above last step | **The range runs round through step 15 to step 0** (first 14, last 1 is four steps), so no pair of settings is invalid. |
| 16 | Pendulum | **Each end is played once per cycle** (0 1 2 3 2 1). Whether the A-154 does the same is unverified. |
| 17 | Addressing state | **A count of steps played in the cycle**, not a step number, so reset and end-of-cycle are the same in every direction. A change of direction or range mid-cycle jumps to the matching point of the new cycle (ARCHITECTURE open item 7). |
| 18 | One-shot | **One cycle, then nothing begins; the clock keeps running.** A reset, or going back to looping, starts the pattern again on the beat. |
| 19 | Reset | **Deferred, as on the A-154**: the first step plays at the next step boundary, the clock is not moved, and holding reset repeats the first step. Kept in phase 6 (decision 63). |
| 20 | Scales and root | **Chromatic, major, natural minor, major and minor pentatonic**, root 0 to 11 semitones; a pitch is 0 to 45 semitones above the lowest. A scale is one table row. No octave control yet. |
| 21 | Controls with no hardware | **Fixed in `app.c`**: all 16 steps, forward, looping, chromatic, root 0, never reset. They are ordinary core inputs, so giving them hardware touches the app and an adapter only. |
| 22 | Choosing between engines | **A `switch` in `seq.c`**, not a table of function pointers. `engine.h` sets the two-function pattern and the shared types. |
| 23 | The log line | **`Step 3 Note = 11` gives the pitch in semitones**; a rest is `Step 5 Rest`. |
| 24 | Scale tables | **In RAM as well as flash** (51 bytes). Flash only would need a compiler extension, which the core may not use. |

**The move to SPI.** 25 is the one to check first, on the board.

| # | Decision | Outcome |
|---|---|---|
| 25 | SPI mode | **Mode 0**, MSB first. It samples on the edge the 74HC165 shifts on, which works because the chip's output changes a little after the edge, and the MCP4822 and 74HC595 take the same mode, so the bus never changes mode. Mode 2 would be safer for the 165 alone. **Unverified on hardware** (section 8). |
| 26 | SPI clock | **1 MHz.** 4 MHz is one bit in `spi.c` if the wiring proves clean. |
| 27 | SS (PB2) | **An output held high from the start**, so the SPI cannot drop out of master mode. It is the DAC chip select: `spi.c` sets it up, `cv_output.c` drives it. |
| 28 | MOSI (PB3) | **An output, set by `spi.c`**, because a bus pin belongs to the driver. |
| 29 | A failed panel read | **Leaves the caller's buffer untouched**: the adapter reads into its own bytes and copies on success. |

**Phase 3.** 30 to 32 are the ones that are heard.

| # | Decision | Outcome |
|---|---|---|
| 30 | Gate length | **A number of clock pulses from the start of the step**, so it scales with the tempo. One setting for the pattern, read when a step begins; **fixed at 12 (half a step)** in `app.c`. |
| 31 | Ties | **A length of a whole step (24) or more ties consecutive notes** into one gate; a rest ends it. It falls out of the arithmetic. A tie for one step comes from the engine (decision 77); a length for one step is not built. |
| 32 | Clock output | **One pulse per step, high for the first half.** It follows the clock, not the pattern: it runs through rests and after a one-shot has finished. That is 4 pulses per quarter note. |
| 33 | A step that cannot be read | **No gate.** The step still takes its turn and its clock pulse. |
| 34 | Reset and a held note | **A reset does not cut the gate short.** Stopping with the run button does. |
| 35 | A step begun late | **Its gate is counted from when the step was due**, and does not open if that time has passed. The clock output stays low while steps are owed. |
| 36 | Applying the outputs | **Both pins are written on every tick, before the log**: gate, then clock. Active high. If the buffer stage inverts, the inversion goes in the two pin adapters. |
| 37 | Ports | **Two, `gate_output_port` and `clock_output_port`**, one function each. The pins are set up in `register_init.c`. |

**Phase 4.** 40 and 42 are the ones to check on the board.

| # | Decision | Outcome |
|---|---|---|
| 38 | DAC setup | **Channel A, gain 2, internal reference**: 1 mV a step, 0 to 4.095 V, so the 0 to 3.75 V of the pitch range needs no gain in the buffer. LDAC is tied low. |
| 39 | The port | **`cv_output_port`, one function, in nominal millivolts** (0 to 4095), not a DAC code, so that another DAC is only another adapter. |
| 40 | Lowest pitch | **0 V.** A DAC stops a few millivolts short of 0 V, so the lowest note may read slightly sharp; a small offset in the table would cure it, at the price of the same off the top. To look at with a meter. |
| 41 | Calibration table | **Five entries, one per octave, with straight lines between.** The ideal table (0, 1000, 2000, 3000, 4000) is used until a measured one is stored. The table may fall as well as rise, so an inverting buffer is only a different table. |
| 42 | When the DAC is written | **On every tick, changed or not**, ahead of the gate, so the apply step takes the same time every tick and a failed write is retried within a millisecond. The cost is 20 µs of bus activity a millisecond beside an analogue output. If that can be heard, write only on a change (a few lines in `cv_output.c`) and accept gate edges about 22 µs later on note ticks. |
| 43 | Pitch and gate | **In the same tick, pitch first**, so the gate leads the settled pitch by a few microseconds. Holding the gate back a tick in the core is the fix if it is ever wanted. |
| 44 | Rests | **The pitch holds.** Only a note changes it. 0 V until the first note. |
| 45 | A failed write | **Logged once per step, after the note.** The note plays at the old pitch; the next tick writes again. |
| 46 | The chain during a DAC write | **Left to shift.** The 74HC165s have no chip select; they are loaded afresh before each read. |

**Phase 5.** 47 was the owner's choice.
48 to 52 are met at the panel: go through them with the board and a meter.

| # | Decision | Outcome |
|---|---|---|
| 47 | How calibration is operated | **The panel switches and the tempo pot, with no new hardware.** |
| 48 | Entering the mode | **A reserved panel pattern, looked at once, at power-on**: the last four steps reading 10, 5, 10, 5. The mode lasts until the power is cycled. |
| 49 | Choosing the point | **The first step's switches, as a number 0 to 4**: point n is the voltage for n octaves above the lowest pitch. |
| 50 | Trimming | **The pot adds −64 to +63 mV to the point's nominal value**, 1 mV for every 8 counts, no hysteresis. That covers a gain error of 1.6 % at 4 V; a wider range is two constants in `cal.c`. |
| 51 | Storing | **Switching on the lowest switch of the last step** (off and on again, since the pattern has it on). Only that point changes. The procedure is "turn the pot until the meter reads n.000 V, then flip the switch". |
| 52 | Showing that it happened | **The gate is high in the mode and goes low for 250 ms after a store.** The debug build logs `Cal point 2 = 2013`. |
| 53 | The record | **12 bytes at EEPROM address 0**: a version byte, five 16-bit entries low byte first, a CRC-8 (polynomial 0x07). **One copy**: power lost during the 40 ms of a full write loses the whole calibration, not one point. A second copy would cure that. |
| 54 | Loading | **Once, at start-up.** Anything that is not a valid record leaves the ideal table and is not an error. |
| 55 | The storage port | **`nv_port`: a read that returns at once, a write that is only started, and a poll** that deals with a byte a pass and skips one that already holds the value. A second write while one is going is refused with `ERR_BUSY`. Nothing reports completion. |
| 56 | Interrupts during a write | **Held off for the two instructions that start each byte.** |
| 57 | Reset cause | **Read and cleared once at start-up, and logged**, a bit for each cause (1 power-on, 2 reset pin, 4 brown-out, 8 watchdog). |
| 58 | The start-up log line | **`Reset = 2 Cal = 1`, kept to 19 characters** so that it has gone before the first step's line is queued. |
| 59 | Where the mode sits | **A core module of its own, `cal`, beside `seq`.** The app calls `seq_tick()` only when `cal_tick()` says the mode is not active. |

**Phase 6.** 61 was the owner's.
62 and 63 were put to the owner and not answered, so the recommendation was built; go through those two first.
64 to 67 and 72 are the ones to check with a real clock patched in.

| # | Decision | Outcome |
|---|---|---|
| 60 | Choosing the clock | **Superseded by decision 131.** It was a sync switch on PD3. |
| 61 | The pot on the external clock | **A ratio to that clock**: seven bands give a step to every 4, 3 or 2 edges, a step an edge, and 2, 3 or 4 steps an edge. No hysteresis at the six boundaries. Gate length and swing were the other uses considered. |
| 62 | The run button | **A toggle; the board powers on running.** Stop cuts the gate at once and holds the clock output low; the pitch holds. Start goes back to the first step with the clock restarted. The alternative was a pause that carries on where it stopped. |
| 63 | The reset input | **Deferred** (decisions 19 and 34): on the external clock the first step plays at the next sync edge. Ignored while stopped. The alternative was an immediate restart. |
| 64 | An external edge | **One edge is one step at a ratio of one.** Exactly 24 × multiply pulses are given from one sync edge to the next, spaced by the last gap. A late edge is waited for one pulse short; an early one gets all that is left at once, and no step is dropped. An edge under half the last gap has its step held back one tick with gate and clock low, so each has a rising edge of its own (the owner's choice, over letting the two steps run together). |
| 65 | A step with no spacing to go by | **Taken to be 20 ms long**: the first after the clock starts, and the first after a gap of 4 s or more. Two constants in `clock.c`. |
| 66 | A change of clock | **Does not restart the pattern.** |
| 67 | How an edge is seen | **A flag the MCU latches, polled once a tick, with its interrupt off**: Timer/Counter1's capture flag for the clock (noise canceller on), external interrupt 0's flag for reset. A step begins 1 to 2 ms after its edge. **Unverified on hardware** (section 8). |
| 68 | The input stage | **Assumed to invert**, with the MCU's pull-up, so the firmware looks for a falling edge. Not designed; if it does not invert, it is one bit in each init function. The run button closes to ground. |
| 69 | Debounce | **20 ms, in the core**, from readings every tick. The first reading is taken as it stands, so a button held at power-on is not a press. |
| 70 | Ports | **One function each**: `clock_input_port` and `reset_input_port` report and forget an edge, `run_input_port` gives a level. Read on every tick. |
| 71 | The first step | **A whole step**, after reset and after the run button alike. |
| 72 | Pins | **PB0 clock in, PD2 reset in, PD7 the run button.** |
| 73 | The log line | **`Transport = 2`**, on a change: 0 stopped, 1 the internal clock, 2 the external. |

**Phase 7, the stage engine.** 74 to 76 were the owner's.
78 to 81 are the ones that are heard.

| # | Decision | Outcome |
|---|---|---|
| 74 | The mode switches | **The ninth 74HC165 goes at the far end of the chain**, so the steps stay where they were. Until it is wired the firmware reads all eight as off. |
| 75 | Choosing the engine | **The low three mode switches, as a number**, decoded in the core: 0 plain, 1 stage, 2 Klee, 3 chance, 4 Cartesian, 5 Euclidean; 6 and 7 play the plain one. |
| 76 | A change of engine | **Read when a step begins; the new engine starts from the beginning.** The alternative was each engine keeping its place. |
| 77 | A tie | **A flag in `engine_step_t`**: the sequencer gives a tied note a gate of a whole step. No change to the gate module. |
| 78 | The stage engine's panel | **Steps 0 to 7 are the stages' notes, steps 8 to 15 their controls**: low two switches the length (1 to 4 steps; the M185 has 1 to 8), high two the gate mode. |
| 79 | The gate modes | **0 single, 1 repeat, 2 hold, 3 rest**, so that a second row all off is a plain pattern of eight steps. |
| 80 | How long a hold lasts | **Until the gate length runs out in the stage's last step.** It does not tie into the next stage unless the gate length is a whole step. |
| 81 | A stage's steps | **The length is read on the stage's first step and kept; the note and the mode on every step.** |
| 82 | The range, for stages | **First and last step are each taken as a stage** (modulo 8). Direction, pendulum and one-shot count stages. |
| 83 | The log line | **`Step 2 Note = 8` gives the stage**, once for each step it lasts. |
| 84 | Its name | **`engine_stage`.** |

**The rest of phase 7.** The owner chose Klee as the next piece.
86, 87 and 91 depart from section 3.

| # | Decision | Outcome |
|---|---|---|
| 85 | Mode switches 3 to 7 | **Belong to the engine chosen.** The pattern controls of decision 21 still have no hardware. |
| 86 | The Klee panel | **A stage's highest switch is its pattern switch and its low three its value, 0 to 7.** Sixteen nibble values would leave nowhere to load the pattern from. |
| 87 | The Klee note | **The sum of the active stages' values, limited to 15**; 0 is a rest. |
| 88 | Load | **The first step after a start, a reset or a change of engine, and the step after mode switch 3 is moved either way** (a toggle has no level that means load). |
| 89 | Invert, merge | **Mode switches 4 and 5.** Invert turns the recirculating bit over (a loop of 32). Merge ties every note to the next step. |
| 90 | Gate buses | **None**: one gate output. A second gate is where a bus would go. |
| 91 | The Turing Machine | **Not an engine: mode switches 6 and 7 of Klee**, a chance of 0 to 3 in 8 that the recirculating bit is turned over. 0 is the lock. |
| 92 | The random numbers | **One 16-bit LFSR in the sequencer's state, seeded with a constant**, so the same choices after every power-on. |
| 93 | Gate probability | **The chance engine (3)**: mode switches 3 and 4 a chance of 0 to 3 in 4 that a step is a rest; switch 5 picks each step at random from the range. |
| 94 | Cartesian | **Engine 4**: a 4 by 4 grid, x moving every 1 to 4 steps and y every 4 to 1, from the one clock. |
| 95 | Euclidean | **Engine 5**: 1 to 16 beats over a bar of sixteen. Other bar lengths and rotation are not built. |
| 96 | Per-step time | **Not built.** The stage engine's lengths cover most of it. |

**Phase 8.** 98 and 99 are the ones seen.

| # | Decision | Outcome |
|---|---|---|
| 97 | How the lights are written | **Two bytes sent ahead of each panel read, latched by the edge that ends the 74HC165 load pulse**, PB1 serving both chains. The lights change once a scan, up to 8 ms late. Unverified on hardware. |
| 98 | What is lit | **The step that last began, while the clock output is high.** Dark while stopped and in calibration mode. A light on for the whole step could not show the same step played twice. |
| 99 | The Klee engine | **The lights show the register**, every active stage. |
| 100 | The port | **`led_output_port`, one function taking sixteen bits**, implemented in `shift_reg_reader.c`, the one file that may touch PB1. |
| 101 | Bit order | **Step 0 on QA of the chip nearest the MCU**; a 1 lights the LED. |

**Phase 9.** 111 had a real cost either way and was confirmed by the owner.

| # | Decision | Outcome |
|---|---|---|
| 102 | The watchdog's timeout | **About 250 ms**, reset only. One constant in `watchdog.c`. |
| 103 | Where it is kicked | **Once a tick, in the app, and nowhere else.** |
| 104 | When it is started | **In `platform_init()`, after everything that can fail and before the timebase.** |
| 105 | The port | **`watchdog_port`, one function.** Starting it is the platform's business. |
| 106 | The bootloader | **Still the stock one on the board**, with which a watchdog reset does not recover until the power is cycled (ARCHITECTURE open item 15). |
| 107 | What is a crash | **A start-up with the watchdog among its reset causes**, and nothing else. The core decides it. |
| 108 | The record | **Five bytes: a number, the reset cause, the place, a count, and a CRC-8** that starts from the layout's version. |
| 109 | The log | **Eight records as a ring, 40 bytes at EEPROM address 16**, with no pointer: the newest is the first not followed by the one numbered above it. |
| 110 | A crash that repeats | **Raises the count of the newest record, up to 255, and then writes nothing**, so a firmware that hangs on every start does not wear a byte out. |
| 111 | Reading it | **The debug build logs the newest record at every start-up** (`Crash 3 Place = 5 Count = 2`), so a board that crashed on the release image is read by uploading the debug one. It costs about 0.3 ms at every start. The alternative was to read it only after a watchdog reset. **Confirmed.** |
| 112 | The breadcrumb | **One byte in `.noinit`, behind `breadcrumb_port`**: 1 start-up, 2 applying outputs, 3 gathering inputs, 4 in the core, 5 between ticks. The numbers are stored, so never reuse one. |
| 113 | When it is written | **At start-up, before the first tick, as one `nv_write()`.** |
| 114 | The checksum | **A core module, `crc8`, shared with the calibration record.** |
| 115 | Not built | Taking the reset cause from where Optiboot leaves it, without which the log records nothing on that bootloader; clearing the log or reading the older records from the firmware; recording failed port calls; a time with each record. |

**Power.** The owner's, with the parts list (`docs/PARTS.md`).

| # | Decision | Outcome |
|---|---|---|
| 116 | Power | **USB for now, with a Eurorack supply a possible later step, so the analogue stages are to work from either.** That bears on the pitch buffer (a rail-to-rail op-amp, to reach 0 V on a single 5 V rail), the gate and clock level (5 V from USB), and where 5 V comes from (never both sources at once without something to keep them apart). |

**Phase 10, MIDI out.** 117 to 119 were the owner's.
121 to 125 are the ones a receiving instrument shows.

| # | Decision | Outcome |
|---|---|---|
| 117 | MIDI in two pieces | **Out first; in follows.** In is the larger half: a receive interrupt and ring buffer, a parser in the core, MIDI clock and Start/Stop as a clock source, and what a received note means. |
| 118 | The lowest pitch | **Note 24, C1**, so the range is notes 24 to 69. One constant in `midi_out.c`. |
| 119 | A stop | **A Stop is sent and the clock carries on at the tempo.** The alternative was to send nothing while stopped. |
| 120 | The image | **A third one, `CONFIG=midi`, with the log given up in it.** Two link-time pairs, no macro. The generator is core code in every image; debug and release hand its bytes to an adapter that discards them. |
| 121 | The messages | **Channel 1, Note On at velocity 100, Note Off as 0x80 with velocity 0, no running status.** Constants in `midi_out.c`. |
| 122 | A start | **A Start, then a clock at once** on the internal clock. On the external clock the first clock waits for the first edge, so that clocks fall on step boundaries. A change of clock sends nothing. |
| 123 | The clock | **One clock for every 4 of the sequencer's pulses**, six a step, following the edges and the ratio on the external clock. |
| 124 | The clock while stopped | **At the pot's tempo until an external edge comes; none from then on.** |
| 125 | Ties | **A tied note at another pitch sends the new Note On and then the old Note Off; at the same pitch it sends nothing.** |
| 126 | A full queue | **Nothing is dropped.** `midi_output_port` gives the room, queues bytes, and sends a byte a pass. The core sends only whole messages that fit, at most 8 bytes a tick, and owes the rest in a fixed order: Stop and Start, clocks, Note On, Note Off. A Note On still waiting when its gate falls is given up, as are clocks owed from before a start. The adapter's queue is 32 bytes. |
| 127 | When it is sent | **Queued in the apply step, after the outputs and lights and ahead of the log.** On the emulated board a Note On starts 0.40 ms after its gate rises, 0.73 ms behind a Start and a clock. |
| 128 | The USART | **Double speed, divisor 63: exactly 31250 baud.** The receiver is enabled though nothing reads it, so both images use every bit name in the shared header (otherwise a rule 2.5 finding). |
| 129 | Calibration mode | **Nothing is sent.** |
| 130 | Not built | MIDI in; a control for the channel, velocity or lowest note; running status; Continue; Song Position; All Notes Off at start-up; a second note for a second gate. |

**Modulation, stage 1.** 131 to 139 were the owner's; 133 to 136 are the owner's own design.
137 departs from what the owner first said and was put to them with the plan.
140 to 142 are the ones that are heard.

| # | Decision | Outcome |
|---|---|---|
| 131 | Choosing the clock | **The edges choose; the sync switch is gone**, and PD3 is free. The first external edge takes the pattern over, until it is stopped. |
| 132 | The way back to the internal clock | **The run button: a stop and a start.** Chosen over going back after about 4 s without an edge, and over a switched jack. |
| 133 | One pot, many amounts | **An 8-way coded rotary selector says which of eight amounts the one pot sets.** A tenth 74HC165 carries it on its low three inputs; five are spare. |
| 134 | Pickup | **After the selector moves, the pot changes nothing until it has been turned to meet the amount held** (within 8, one constant in `pickup.c`, or passing it). |
| 135 | Switching a type on | **No enable button: an amount of 0 is off.** |
| 136 | Showing it | **One light, as bright as the amount the selector is on.** |
| 137 | Which position is the tempo | **Position 0**, because a board with no selector wired reads 0. 1 to 3 are the note, the gate length and the step chance; 4 to 7 are kept for step position, amplitude, attack/release and vibrato. Four constants in `app.c`. |
| 138 | At power-on | **The tempo is where the pot is, and every other amount is 0. Nothing is stored.** |
| 139 | The LFO input with nothing plugged in | **Reads as no modulation.** The signal swings either side of 512 of 1023, with a dead band of 16. Chosen over a jack that feeds a fixed voltage when empty, because an internal LFO is to come. |
| 140 | The note | **Moved by up to seven places in the scale**, kept within 1 to 15. It never makes or moves a rest. A constant in `seq.c`. |
| 141 | The gate | **Up to half a step longer or shorter**, kept within one pulse to a whole step. |
| 142 | The step chance | **Above the centre only, makes steps drop out**, up to 255 times in 256. Drawn only when the chance is not 0, so the engines' own choices are unchanged without modulation. |
| 143 | When the LFO is looked at | **Read every 8 ms, used as each step begins**: a sample and hold. Vibrato and an envelope would need it every tick. |
| 144 | Pins and parts | **The LFO on ADC7; the level light on PD6, by pulse width from Timer/Counter0 at 977 Hz.** A failed LFO read is taken as no modulation and not logged. |
| 145 | Reuse | **`pickup`, `bank` and `mod` know nothing of the sequencer**, at the owner's asking, who may build another board around them. |
| 146 | Not built | The later stages, in order: (1) step position; (2) the DAC's channel B as a level and envelope output, amplitude setting its peak and the MIDI velocity, attack/release its slopes; (3) vibrato; (4) an internal LFO, with a rate pot on PC0 where 0 Hz is off, summed with the external one; (5) eight profiles of the amounts in the EEPROM, chosen by the selector, the one selected loaded at power-on if it holds anything, with one button: a click recalls, a hold of 1 s saves, of 5 s clears the selected profile and of 10 s clears them all, acting on release, the calibration untouched; (6) a status light on PD3, steady when on, flickering once for a recall, twice for a save, three times for a clear and four for clearing all; (7) possibly an enable button for a type. Also not built: a log line for a failed LFO read, and any input stage for the LFO. |

**Modulation, stage 2: step position.** Settled on 2026-10-11, ahead of any code.
All eight were the owner's choice, from options put to them.

| # | Decision | Outcome |
|---|---|---|
| 147 | The unit | **Places in the range, not steps of the pattern.** The offset wraps inside first to last, so nothing outside the range sounds. A place is counted along the range (towards the last step), whatever the direction. Chosen because steps of the pattern cannot hold in the stage engine, whose steps 8 to 15 are controls. |
| 148 | How far | **Half the range each way at full depth**, scaled to the range's length and rounded down: 8 of 16, 4 of 8, 2 of 4 or 5, nothing in a range of one. The LFO's whole swing covers every place once. Chosen over a fixed 7 or 8 places, which wrap more than once round a short range. |
| 149 | What an engine is handed | **The depth and its direction, never the LFO or the amount.** The amount applied to the LFO (`mod_depth()`) goes in through `engine_inputs_t` as plain values, and each engine turns it into places of its own cycle. `engine.h` does not include `mod.h`. |
| 150 | The count | **The offset moves the step given, never the count.** The pattern does not slip, a one-shot ends after the same number of steps, and with the LFO at its centre the pattern is where it would have been. A depth of 0 changes nothing and draws no random number. |
| 151 | Order | **Applied in the engine, before the note is read.** The step chance and the note's shift (140, 142) act on the step played, and the step given, its light and the log line are that step. Unlike 140 it can land on a rest or leave one. In the Euclidean engine it moves which note sounds, not whether; in the chance engine it moves a shuffled step too. |
| 152 | The stage engine | **Stages in the stage range**, up to half of it each way, taken when a stage begins, like its length: a hold or a repeat stays on one stage. |
| 153 | The Klee engine | **The register is turned for that step's sum**, by up to 8 stages either way. The register kept and its count are untouched, and the lights show it as turned. Chosen over ignoring the offset. |
| 154 | The Cartesian engine | **The cell's number (row × 4 + column) is moved round sixteen**, by up to 8 either way, the counters untouched. With the engine's switches off it is the plain engine, modulated or not. Chosen over moving along the row only, and over ignoring it. |

**Modulation, stage 3: the DAC's channel B.** Settled on 2026-10-11, ahead of any code.
155 to 160 and 164 were the owner's choice, from options put to them; 161 to 163 were left to recommendation.

| # | Decision | Outcome |
|---|---|---|
| 155 | Position 5, amplitude | **An LFO amount, like 1 to 4.** With the amount at 0 the peak is full scale (4.095 V) and the MIDI velocity 100, so power-on is as it was and 138 holds. Chosen over the pot setting the peak itself, which cannot be set by hand here. |
| 156 | Which way | **Down only.** Below its centre the LFO lowers the peak and the velocity together, to 0 V and velocity 1 at the fullest; above it both stay where they are. Chosen over raising the velocity above 100, and over either side lowering. |
| 157 | The shape | **One shot per note: attack, then decay at once**, whatever the gate length. Nothing sustains. Chosen over an envelope that follows the gate, and over a level with no envelope. |
| 158 | Position 6, attack/release | **One time for both slopes**, 0 at power-on, which is instant. Position 7 stays free for vibrato. Chosen over release alone, and over a pot split between the two. |
| 159 | The time | **Milliseconds, up to about a second a slope, on a curve**: the amount brought to 0 to 255, squared and divided by 64, so the bottom of the pot is fine and the top coarse (a quarter of the way up is 62 ms, half way 252 ms, the top 1016 ms). Counted in ticks, an exception to lengths being counted in clock pulses, because a slope should be the same at any tempo and smooth on the external clock. The curve was the owner's asking. |
| 160 | What starts it | **Each rise of the gate**, from wherever the level is, so nothing clicks. A step tied to the one before is the same gate and does not start it again, as for MIDI out. |
| 161 | When the LFO is looked at | **As the note begins**, like everything else it moves (143): the peak is that note's for the whole of its envelope, and is its velocity. |
| 162 | The slopes | **Straight lines**, to the peak and back to 0 V, worked out every tick and written to channel B every tick after channel A. |
| 163 | Stopped, and in calibration mode | **A stop does not cut an envelope short**: it runs out as it would have. In calibration mode channel B stays at 0 V. |
| 164 | A time of 0 | **Channel B follows the gate**: at the peak while the gate is open and at 0 V while it is closed, so a VCA on it works from power-on. It is the time in milliseconds that counts, which is 0 for the lowest thirty-odd counts of the pot. The owner's choice, over holding the peak and over staying at 0 V. |

**Modulation, stage 4: vibrato.** Settled on 2026-10-12, ahead of any code.
165 to 168 were the owner's choice, from options put to them; 169 to 171 were left to recommendation.

| # | Decision | Outcome |
|---|---|---|
| 165 | How far | **Two semitones either way at full depth.** A constant at the top of `seq.c`, kept with the other ranges so that it moves with them into the board's modulation configuration. Chosen over one semitone, and over an octave. |
| 166 | Which way | **Both**: up while the LFO is above its centre and down while it is below, like the note and the gate length (140, 141). Chosen over up only and over down only. |
| 167 | When the LFO is looked at | **Every tick.** The LFO alone leaves the 8 ms scan and is read on every tick, after it, which is about 0.1 ms more of every tick; the other five moves still take it as a step begins (143, 161). Chosen over a line drawn in the core between readings 8 ms apart, and over 8 ms stairs. |
| 168 | MIDI | **The CV only.** The MIDI note is the note as set and no pitch bend is sent. Chosen over pitch bend, which costs flash and only matches a receiver set to the same range. |
| 169 | The size of a semitone | **The calibration table's**: the pitch goes part of the way to the value the table gives the pitch two semitones off, so the swing is right wherever the output stage is not. Nothing is below the lowest pitch, so a note within two semitones of it goes down no further than to it. |
| 170 | What it moves | **The value given, never the note.** The step, its log line and the next tick all start from the note's own pitch. A new kind of move in `mod` (`mod_scale()`), since a CV value does not fit the byte `mod_shift()` moves. |
| 171 | Rests, a stop, calibration mode | **It goes on through a rest and a stop**, as a VCO with no VCA on it would be heard to, and an envelope's tail (163) keeps its vibrato. In calibration mode there is none. Before the first note it is about the lowest pitch. |

**Modulation, stage 5: an LFO of the sequencer's own.** Settled on 2026-10-12, ahead of any code.
172 to 175 were the owner's choice, from options put to them; 176 to 178 were left to recommendation. The rate pot on PC0, 0 Hz being off and the sum with the external LFO were settled in 146.

| # | Decision | Outcome |
|---|---|---|
| 172 | The shape | **A sine.** Chosen over a triangle, which needs no table, and over several shapes on switches. |
| 173 | The rate | **Free, in Hz, on a curve**: the pot's reading over 4, squared, over 48, in 65536ths of a cycle a tick. Off below a reading of 28, then a cycle in 65.5 s, 1.3 Hz a quarter of the way up, 5.2 Hz half way and 20.7 Hz at the top. Chosen over a rate tied to the tempo, and over a straight line. |
| 174 | The sum | **The two swings added, and the sum kept within 0 to 1023.** With nothing plugged in the sequencer's own LFO is all there is; with both, the ends clip. Chosen over halving each, and over the external one taking over. |
| 175 | The cycle | **Free-running**: a stop, a start and a reset do not touch it. Chosen over beginning again on a start, and over beginning again on every note. |
| 176 | How the sine is made | **A quarter-wave table of 17 points, a straight line between them**, 17 bytes of RAM and flash, within about half a percent of a sine. The swing is the whole of the signal's, 511 either way; the amounts say how much of it each thing gets. |
| 177 | Off | **At its centre, and the cycle put back to its start**, so that turned up again it begins from the centre going up. A rate pot that cannot be read leaves the last rate. It does not run in calibration mode. |
| 178 | When the rate pot is read | **In the 8 ms scan, in place of that tick's read of the modulation input**, which amends 167: the external LFO is read on seven ticks in eight and held for the eighth. Every tick then has the conversions it had before, and no timing bound moved. Chosen over a third conversion in the scan, with which the first tick after power-on ran past its millisecond on the emulated board, and over a tick of its own for the rate pot, which put MIDI bytes up to a quarter of a millisecond later on that tick. |
