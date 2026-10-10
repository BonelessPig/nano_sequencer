# nano_sequencer roadmap

Written 2026-10-10 from four research passes (existing sequencers, hardware emulation, the output-stage hardware, and Claude Code context cost) plus a working emulation spike, and updated the same day with the owner's decisions (section 2). The emulated board (`make sim`), phase 1 (the timebase) and phase 2 (addressing, note mapping and the engine interface) are built; everything else here is still a plan.

Each claim that matters is marked **verified** (read from a datasheet or primary manual, or run on this machine), or **unverified** (from memory or a secondary source; check before relying on it). The full list of unverified items is in [section 8](#8-not-verified).

## 1. Summary

- **Where it goes.** A 16-step CV/gate sequencer with one pitch output, gate, clock in and out, reset, and a set of selectable sequencing engines (plain, Metropolis-style, Klee-style, random) that all reinterpret the same 64 panel switches. Every engine is pure core logic, so it is cheap to add and fully host-testable.
- **What limits it.** Pins and the single USART, not flash or RAM (after phase 2 the debug image uses 2534 of 30720 bytes of flash and 382 of 2048 bytes of RAM).
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
| 4 | Shift registers | **Move the 74HC165 chain to hardware SPI** in phase 4. Frees INT0 and INT1 and shares the bus with the DAC and the LED chain. Three signals to rewire. |
| 5 | Output timing | **Apply outputs at the tick boundary**: each pass applies what the previous pass computed, then gathers and computes. A constant 1 ms latency for near-zero jitter. The tick rule in CLAUDE.md changes with phase 1. |
| 6 | Rests | **Note 0 is a rest**, leaving 15 pitches. |
| 7 | Choosing a mode | **A ninth 74HC165** (8 more switches, no MCU pins). The core's raw input grows from 8 to 9 bytes. |
| 8 | Pitch DAC | **Open.** Candidates: MCP4822 (SPI, internal 2.048 V reference), MCP4725 (I2C, reference is the supply), PWM, R-2R. The point to weigh: every option but the MCP4822 takes its reference from the USB 5 V rail, and a 1 % supply change is about 48 cents at 4 V. Phase 4's SPI move does not depend on this; the DAC adapter does. |
| 9 | Instruction layout for Claude | **Nested `CLAUDE.md` files per folder.** The C coding standard and a line length rule (80 soft, 100 hard) are also in the user-level `~/.claude/CLAUDE.md`. |

Taken while building phase 1, as the simplest thing that met the plan. Each is one constant or a few lines to change.

| # | Decision | Outcome |
|---|---|---|
| 10 | Tempo range | **30 to 285 BPM, linear**: 30 plus the pot reading divided by 4. A step is a sixteenth note (24 pulses). |
| 11 | Input scan rate | **Every 8 ticks (125 Hz)**, panel and pot together, scheduled by the app. |
| 12 | Log when the queue is full | **Drop the whole message.** The queue is 128 bytes, enough for the two longest messages. Failed reads are logged once per step, not once per scan. |
| 13 | Steps owed after a stall | **Begin one per tick until caught up**, none skipped. |
| 14 | Start-up | **The first tick begins step 0**, so that step is one tick short. To revisit with `transport` in phase 6. |

Taken while building phase 2, on the same basis. Each is contained in one core module.

**To reconfirm.** Decisions 15 to 24 were accepted on 2026-10-10 as provisional, to keep the work moving, and have not been reviewed one by one. Go through them again before any of these controls gets hardware (phase 6 for reset, phase 7 for the rest), since that is when they become audible and when changing them stops being free. Remove this note once each is confirmed or changed.

| # | Decision | Outcome |
|---|---|---|
| 15 | First step above last step | **The range runs round through step 15 to step 0** (first 14, last 1 is four steps), so no pair of settings is invalid. |
| 16 | Pendulum | **Each end is played once per cycle** (0 1 2 3 2 1), not twice. Whether the A-154 does the same is not verified. |
| 17 | Addressing state | **A count of steps played in the cycle**, not a step number. Reset and end-of-cycle are then the same in every direction. A change of direction or range mid-cycle jumps to the matching point of the new cycle and does not turn round in place (ARCHITECTURE open item 7). |
| 18 | One-shot | **One cycle, then nothing begins; the clock keeps running.** A reset, or going back to looping, starts the pattern again on the beat. |
| 19 | Reset | **Deferred, as on the A-154**: the pattern goes back to its start and the first step plays at the next step boundary. The clock is not moved, and holding reset repeats the first step. A reset that also restarts the clock belongs with `transport` in phase 6. |
| 20 | Scales and root | **Chromatic, major, natural minor, major and minor pentatonic**, root 0 to 11 semitones. A pitch is 0 to 45 semitones above the lowest. A scale is one table row to add. **No octave control yet**: its range depends on the DAC (decision 8). |
| 21 | Controls with no hardware | **Fixed in `app.c`**: all 16 steps, forward, looping, chromatic, root 0, never reset. They are ordinary core inputs, so giving them switches or pots later touches the app and an adapter, not the core. |
| 22 | Choosing between engines | **Not built yet.** `engine.h` sets the two-function pattern and the shared types; `seq.c` calls the plain engine directly. The selector comes with the second engine in phase 7, as a `switch`, not a table of function pointers. |
| 23 | The log line | **`Step 3 Note = 11` now gives the pitch in semitones** (the note value less one in the chromatic scale), and a rest is `Step 5 Rest`. |
| 24 | Scale tables | **In RAM as well as flash** (51 bytes). Flash only would need a compiler extension, which the core may not use. |

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
  | `clock` | Elapsed ticks to musical pulses (internal tempo or external edges) |
  | `transport` | Stopped, running, external sync, as an explicit state machine |
  | `address` | Next step: direction, first and last step, one-shot, reset |
  | `engine_*` | One per engine: plain, 8x2, Klee. Same interface |
  | `note_map` | Scale degree, root and octave to a semitone |
  | `pitch_cal` | Semitone to DAC code through a calibration table |
  | `gate` | Gate length, ties, ratchets, in pulses |
  | `debounce` | Buttons |
  | `prng` | One LFSR shared by every random feature |
  | `errlog` | Crash record format |

- **Ports are named for what the core needs**, not for the bus: `timebase_port`, `gate_port`, `cv_port`, `clock_in_port`, `led_port`, `nv_port`, `watchdog_port`. No `spi_port`.
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

In place as of 2026-10-10: `lib/machine.js` (the chip), `parts/hc165.js`, `boards/nano_sequencer.js` (wiring, panel, pot, serial capture) and eight scenarios across the two images. ARCHITECTURE.md describes it.

Still to add, each when a phase needs it:

- **Timers and interrupts in `lib/machine.js`**: Timer/Counter2 added in phase 1. Timer1 when phase 6 needs it.
- **More parts**: clock source and button (phase 6), MCP4822 or whichever DAC is chosen (phase 4), 74HC595 (phase 8). SPI parts hook the byte transfer, not the pins.
- **A pin recorder with assertions** such as `count_rises(pin, t0, t1)` and `period(pin)`, once there is a gate to measure (phase 3). The 74HC165 model counts its own pulses for now.
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

- The driver and device layers in `adapters/target/CLAUDE.md` (phase 4). The ISR and `volatile` rules went in with phase 1.
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
| 3. Gate and clock out | Core `gate` (length in 1/24 step, ties), `gate_port`, clock out | GPIO PD4, PD5 | Buffer IC, resistors, jacks | Sim: pulse widths and counts. Board: logic analyser or LED |
| 4. SPI and pitch CV | SPI driver; 74HC165 chain on SPI (can go first, on its own); DAC adapter once the DAC is chosen (decision 8); `pitch_cal` with the ideal table. First point it plays an oscillator | SPI | The DAC, op-amp buffer, 1 kΩ on MISO | Sim: exact DAC frames, DAC written before gate rises. Board: tuning by ear and meter |
| 5. Calibration and storage | Calibration mode, non-blocking EEPROM adapter, record with version and checksum, reset-cause read | EEPROM | Multimeter | Host: record format. Sim: EEPROM contents. Board: measured octaves |
| 6. Clock, reset and run in | One external edge per step, reset, `transport`, `debounce` | Timer1 capture flag and INT0 flag, polled | Two transistor input stages, button, jacks | Sim: scripted clock source. Board |
| 7. Engines | 8x2 row modes (per-step time, then pulse count and gate mode), Klee (register, sums, buses, merge, invert, load), `prng` with Turing lock and gate probability, Cartesian and Euclidean | More ADC channels | Ninth 74HC165, pots | Host, mostly. Sim for gate timing |
| 8. Step LEDs | 16 LEDs in the same SPI frame as the switch read | None | Two 74HC595s, LEDs | Sim: 595 model. Board |
| 9. Watchdog and crash log | Watchdog kick, `errlog` ring in EEPROM, `.noinit` breadcrumb. Needs decision 3 | Watchdog | None | Sim: watchdog reset and recovery. Board |
| 10. MIDI | `CONFIG=midi`: clock and notes out, then in | USART at 31250 baud (exact), RX interrupt | DIN sockets, optocoupler | Host: parser and generator. Sim: UART bytes |
| 11. Later | 24 PPQN sync with interpolation, second DAC channel, slide, swing and ratchets if not already in | Timer1 timestamps | — | — |

Notes on ordering:

- Phases 2 and 7 are pure core work and can be done at any time, with no hardware. Phase 7 is listed late only because gates and pitch make it audible.
- Phase 4's shift register move changes the hex, so the `cmp` proof does not apply; the sim scenario from phase 0 is the regression check.
- Phase 1 is the largest architectural change (first interrupt, no more blocking). It is first because gate length, ratchets, swing and external clock all depend on it.

### Proposed pin assignment (from phase 4)

| Pin | Use | Pin | Use |
|---|---|---|---|
| PD0, PD1 | UART (log or MIDI) | PB0 (ICP1) | Clock in |
| PD2 (INT0) | Reset in | PB1 | 74HC165 load and 74HC595 latch |
| PD3 (INT1) | Run in or spare | PB2 (SS) | DAC chip select |
| PD4 | Gate out | PB3 (MOSI) | DAC and 595 data |
| PD5 | Clock out | PB4 (MISO) | 74HC165 data, through 1 kΩ |
| PD6 | Accent or second gate | PB5 (SCK) | SPI clock (on-board LED flickers) |
| PD7 | Run/stop button | PC0 to PC3 | CV in, pots |
| ADC6 | Tempo pot (unchanged) | PC4, PC5 | Reserved for I2C |
| ADC7 | Pot or CV in | | |

Where the 328P runs out, in order: one USART (log or MIDI, not both), no on-chip debug, a 10-bit ADC (fine for quantised transpose, not for pitch tracking), three timers. The smallest step up is the ATmega328PB (two USARTs, same register model); any move costs a new adapter folder and leaves the core and ports alone.

## 8. Not verified

Check these before the phase that depends on them.

- **Register bit positions** inside the SPI, EEPROM and watchdog control registers, and the OCR1A address. Addresses, prescaler codes and vector numbers were read from the datasheet; bit positions were not. (Phases 4, 5, 9) The Timer2 ones used in phase 1 are now **verified** against the toolchain's own `iom328p.h` and by the emulator producing a 1 kHz tick from them. That header ships with the toolchain and is a quick check for the rest.
- **`__vector_7` as the Timer2 compare A symbol**: now **verified**. It links, the vector table entry jumps to the handler, and the handler runs on the emulator.
- **Edge flags latching with their interrupts disabled** (for polled clock and reset inputs). (Phase 6)
- **SPI mode for the 74HC165 alongside the DAC**, and one pin serving as both 165 load and 595 latch. Bench-check. (Phases 4, 8)
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
- MCP4802/4812/4822 datasheet (DS20002249B)
- Old Nano bootloader source: https://raw.githubusercontent.com/arduino/ArduinoCore-avr/master/bootloaders/atmega/ATmegaBOOT_168.c
- avr8js: https://github.com/wokwi/avr8js — rp2040js: https://github.com/wokwi/rp2040js
- simavr: https://github.com/buserror/simavr
- Renode: https://github.com/renode/renode
- Wokwi CI: https://docs.wokwi.com/wokwi-ci/getting-started
- avr-tester: https://codeberg.org/pwy/avr-tester
- Claude Code memory, skills, subagents, hooks and costs: https://code.claude.com/docs/en/memory , /skills , /sub-agents , /hooks , /costs
- Books: Grenning, *Test-Driven Development for Embedded C*; White, *Making Embedded Systems*; Pont, *Patterns for Time-Triggered Embedded Systems*; Samek, *Practical UML Statecharts in C/C++*
