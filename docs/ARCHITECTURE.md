# nano_sequencer — architecture and inner workings

How the firmware is structured, what happens from reset to the main loop, how each part works, and what is still open. The standing rules for the codebase are in [CLAUDE.md](../CLAUDE.md) and the `CLAUDE.md` in each source folder; this document describes what is there.

How this was checked: the firmware is compiled with avr-gcc 12.1.0 under `-std=c99 -Wall -Wextra -Wconversion -Wshadow -Werror`, and the same source is compiled and tested on a PC with `make test`, the MCU adapters against fake registers. The refactored firmware has been flashed and run on the board; the later logger rewrite, the 115200 baud setting, the release build, the step advance, the timer tick with its non-blocking loop, and the addressing, note mapping and rests have not yet. Timing figures marked as measured come from the emulated board; the rest are calculated.

## At a glance

| | |
|---|---|
| Target | ATmega328P (Arduino Nano), 16 MHz |
| Flash used | 2534 bytes of 32 KB (debug build); 1800 bytes (release build) |
| RAM used | Debug: 382 bytes static (a 128-byte log queue, 155 bytes of message text and 51 bytes of scale tables copied from flash, 48 bytes of state). Release: 94 bytes (the tables and 42 bytes of state). Plus stack |
| Interrupts | One: Timer/Counter2 compare match A, 1000 times a second. It adds one to a counter and does nothing else |
| Main loop | Never blocks. One sequencer tick per millisecond; outputs are applied at the tick boundary |
| Inputs | 16 steps × 4 bits from eight 74HC165s; one pot on ADC channel 6 for tempo. Both are read every 8 ms |
| Tempo | 30 to 285 BPM, a step being a sixteenth note (500 ms down to 52.6 ms per step) |
| Notes | A step's four switches are its note value: 0 is a rest, 1 to 15 a pitch through a scale and a root |
| Pattern | The core can play any range of steps forward, in reverse or as a pendulum, looping or once, with a reset. The board has no controls for these yet, so the firmware plays all 16 steps forward, looping, in the chromatic scale |
| Outputs | Serial log only (115200 baud, debug build); none at all in the release build. No gate, trigger or CV output yet |
| Tests | 161 host tests in 15 programs (83 for the core, 19 for the app loop, 56 for the seven target adapter sources, 3 for `main.c`) |
| Coverage | 100% of lines (393) and branches (155) in `core/`, `app/` and `adapters/target/`, measured on the PC build by `make coverage` |

## Hardware

What is wired to the chip, and which way each signal goes. The dotted path exists only in the debug build.

```mermaid
flowchart LR
    panel["Front panel<br/>16 steps x 4 bits"] --> chain["8 x 74HC165<br/>shift register chain"]
    pot["Tempo pot"]
    subgraph mcu["ATmega328P on an Arduino Nano, 16 MHz"]
        gpio["Port D<br/>PD2 load, PD3 clock, PD4 data"]
        adc["ADC channel 6"]
        usart["USART0<br/>PD1 transmit"]
        timer["Timer/Counter2<br/>1 kHz tick, no pin"]
    end
    gpio -->|"load and clock pulses"| chain
    chain -->|"64 bits, one at a time"| gpio
    pot -->|"0 V to AVcc"| adc
    usart -.->|"115200 baud"| usb["USB serial chip"]
    usb -.-> pc["PC terminal"]
```

## Structure: ports and adapters

The sequencer logic is a pure core. It never touches hardware: the app gathers inputs through ports, hands them to the core, and applies what the core returns.

```mermaid
flowchart TD
    main["app/main.c<br/>init, then loop forever"] --> app["app/app.c<br/>gather, tick, apply"]
    app -->|"calls"| core["core/: seq, clock, engine_plain,<br/>address, note_map, panel<br/>pure logic, no hardware"]
    app -->|"calls through"| ports
    subgraph ports["ports/ (headers only)"]
        p_platform["platform_port.h"]
        p_step["step_input_port.h"]
        p_tempo["tempo_input_port.h"]
        p_time["timebase_port.h"]
        p_log["log_port.h"]
    end
    subgraph target["adapters/target/ (linked into the firmware)"]
        t_init["init.c<br/>register_init.c"]
        t_shift["shift_reg_reader.c"]
        t_adc["analog_reader.c"]
        t_time["timebase.c"]
        t_serial["serial_logger.c<br/>debug build"]
        t_null["null_logger.c<br/>release build"]
    end
    subgraph host["adapters/host/ (linked into the PC tests)"]
        h_ports["host_ports.c<br/>scripted inputs, recorded outputs"]
    end
    t_init -.->|"implements"| p_platform
    t_shift -.->|"implements"| p_step
    t_adc -.->|"implements"| p_tempo
    t_time -.->|"implements"| p_time
    t_serial -.->|"implements"| p_log
    t_null -.->|"implements"| p_log
    h_ports -.->|"implements all five"| ports
```

Solid arrows are calls; dotted arrows show which file supplies the functions a port header declares. The core sits to one side: it calls nothing and nothing but the app calls it.

| Folder | Role | May touch hardware |
|---|---|---|
| `core/` | Sequencer logic. Plain C99; includes only its own headers and `<stdint.h>`, `<stdbool.h>`, `<stddef.h>` | No |
| `ports/` | Headers describing what the app needs from outside: one small header per need | No |
| `adapters/target/` | The ports implemented for the ATmega328P | Yes — the only place |
| `adapters/host/` | The ports faked for PC tests: scripted inputs, recorded outputs | No |
| `app/` | Wires ports to the core; holds `main` | No |
| `tests/` | Host test programs and a small assert-based harness | No |
| `tools/sim/` | An emulated board that runs the built firmware images (JavaScript, not firmware source) | No |

Ports are bound at link time: the firmware links `adapters/target/`, the tests link `adapters/host/`, and both use the same headers. There are no function-pointer tables. The debug and release builds are the same idea one level down: `adapters/target/` holds two implementations of the log port, and the Makefile links one of them (see Build configurations below).

### The ports

| Port | Function | Target implementation |
|---|---|---|
| `platform_port.h` | `platform_init()` | `init.c`: logger, then pin directions and ADC, then the timebase |
| `step_input_port.h` | `step_input_read(p_raw_bits, byte_count)` | `shift_reg_reader.c`: 74HC165 chain |
| `tempo_input_port.h` | `tempo_input_read(p_raw)` | `analog_reader.c`: ADC channel 6 |
| `timebase_port.h` | `timebase_elapsed_ticks()` | `timebase.c`: Timer/Counter2 interrupting at 1 kHz |
| `log_port.h` | `log_step_note(step, semitone)`, `log_step_rest(step)`, `log_error(what, status)`, `log_poll()`, `log_flush()` | `serial_logger.c`: USART0 (debug build), or `null_logger.c`: discards everything (release build) |

Functions that can fail return `port_status_t` (`STATUS_OK` = 0, `ERR_INVALID_PARAM` = 2, `ERR_TIMEOUT` = 4, and so on). The numbers appear in the debug build's serial log, so they are fixed. `timebase_elapsed_ticks()` cannot fail and returns the tick count itself.

## What "bare metal" means here

No source file includes an avr-libc header. Registers are declared in `adapters/target/atmega328p_regs.h` (with `atmega328p_usart_regs.h` for USART0 and `atmega328p_timer2_regs.h` for Timer/Counter2) and placed by the linker. Interrupts are enabled by setting the bit in the status register, and the one interrupt handler is an ordinary C function given avr-gcc's `signal` attribute and the name the vector table expects (`__vector_7`). No compiler builtin is used. There is no `printf`, `memset` or any other library function; the logger converts numbers to text itself. The build uses `-ffreestanding`, so `<stdint.h>`, `<stdbool.h>` and `<stddef.h>` are the compiler's own.

The link step is not library-free. The Makefile links with plain `avr-gcc -mmcu=atmega328p`, so the map file shows:

- `crtatmega328p.o`, which ships with avr-libc. It supplies the interrupt vector table, `__bad_interrupt`, and the `__init` code that sets the stack pointer and calls `main`. Every vector but the reset and the timer tick still points at `__bad_interrupt`.
- From libgcc: `__do_clear_bss`, `__do_copy_data`, `_exit`, and `__udivmodhi4` (16-bit divide, used when converting numbers to decimal).
- `libc.a` and `libm.a` are searched, but nothing is pulled from them.

## Reset to main loop

```mermaid
sequenceDiagram
    participant CRT as crt + libgcc
    participant M as main
    participant A as app
    participant P as ports (target adapters)
    participant C as core
    CRT->>CRT: set stack pointer, clear .bss
    CRT->>M: call main
    M->>A: app_init()
    A->>P: platform_init()
    A->>C: seq_init()
    loop forever: app_run_once()
        A->>P: timebase_elapsed_ticks()
        opt a tick has passed
            A->>P: log_step_note() or log_step_rest() if the last tick began a step
            A->>P: step_input_read(), tempo_input_read() every 8th tick
            A->>C: seq_tick(state, inputs, outputs)
        end
        A->>P: log_poll()
    end
```

`platform_init()` ends by starting the timer and enabling interrupts, so the tick is running before the first pass of the loop. If `platform_init()` fails, `app_init()` logs it, waits for the log to be sent (`log_flush()`, the only blocking call left) and `main` returns the status; execution then falls into libgcc's `_exit`, which disables interrupts and loops forever. Neither target init function can currently fail.

The same thing as states:

```mermaid
stateDiagram-v2
    [*] --> Startup: power on or reset
    Startup --> Init: startup code calls main
    Init --> Running: platform_init returned STATUS_OK
    Init --> Halted: platform_init failed
    Running --> Running: app_run_once, one pass
    note right of Halted
        main returned. libgcc's _exit masks
        interrupts and loops until the next reset.
    end note
```

## The main loop and one tick

`main` calls `app_run_once()` in [`app/app.c`](../app/app.c) forever, and that function never waits. Each pass asks the timebase how many 1 ms ticks have gone by since the last pass.

- **None:** the pass gives the log the chance to send one byte and returns. Between ticks the loop spins through this path, a few microseconds a time.
- **One or more:** the pass runs one tick of the sequencer, in the three steps below, and then polls the log as well. More than one only happens if a pass overran; the count is handed to the core, so no time is lost.

1. **Apply outputs.** What the previous tick computed is acted on first. If a step began, log the step and its pitch, or that it is a rest (or one error if the step read had failed), and an error if the tempo read had failed.
2. **Gather inputs.** Note the elapsed ticks. Every eighth tick (125 times a second) read 8 raw bytes from the step input port and one reading from the tempo port; each read's success becomes a valid flag in `seq_inputs_t`. Between scans the core is given the last snapshot again.
3. **Run the core.** `seq_tick()` moves its clock on by the elapsed ticks and, if that brings a step due, begins it and works out its pitch. The result is kept until the next tick.

Outputs are applied at the start of the next tick, not at the end of this one, so they change at the same point in every tick however long the reads and the core took. The price is that every output is one tick (1 ms) late, always by the same amount. Today the only output is the log; a gate will be applied the same way.

```mermaid
flowchart LR
    subgraph apply["1. Apply outputs (from the last tick)"]
        logn["log_step_note or log_step_rest<br/>if a step began and its note is valid"]
        loge["log_error<br/>if a step began and a read had failed"]
    end
    subgraph gather["2. Gather inputs"]
        tb["timebase_elapsed_ticks<br/>(already read: it started the tick)"]
        sr["step_input_read<br/>every 8th tick"]
        tr["tempo_input_read<br/>every 8th tick"]
    end
    subgraph run["3. Run the core"]
        in["seq_inputs_t<br/>raw_steps: 8 bytes<br/>b_steps_valid<br/>tempo_raw: 0 to 1023<br/>b_tempo_valid<br/>elapsed_ticks<br/>address: direction, first, last, one-shot<br/>note_map: scale, root<br/>b_reset"]
        tick["seq_tick"]
        state[("seq_state_t<br/>clock phase<br/>last_tempo_raw<br/>step_pulses<br/>engine position")]
        out["seq_outputs_t<br/>b_step_started<br/>step: 0 to 15<br/>b_note_valid<br/>b_rest<br/>semitone: 0 to 45"]
    end
    tb --> in
    sr --> in
    tr --> in
    in --> tick
    tick <--> state
    tick --> out
    out -->|"kept until the next tick"| logn
    out --> loge
```

The two read statuses are kept beside the snapshot (not passed through the core), so a failed read can be logged with its status code. A failed read is reported once per step, along with the step it affected, not once per scan.

The last three inputs in the diagram (the addressing controls, the scale and root, and reset) have no hardware behind them yet. The app sets them once, in `set_fixed_controls()`: all sixteen steps, forward, looping, never reset, in the chromatic scale from the lowest pitch. When there are switches or pots for them, that function is what a port read replaces.

A tick is a millisecond, not a step. A step lasts 24 clock pulses, which at the tempo set by the pot is 500 ms down to 52.6 ms, so most ticks compute nothing new and apply nothing. Sixteen steps go once round the pattern.

**Timing, measured on the emulated board** at 150 BPM, where a step is exactly 100 ms:

| | Debug | Release |
|---|---|---|
| Panel scan period | 8 ms, within 0.12 ms | 8 ms, within 0.004 ms |
| Step period, from the first byte of one log line to the first byte of the next | 100 ms, within 0.20 ms | No log; nothing outside the chip shows a step yet |
| One log line on the wire | 1.0 ms (a rest) to 1.6 ms, sent a byte per pass while the loop carries on | — |
| First step logged | 2.3 ms after reset | — |

The step itself begins on a tick boundary in both builds. The 0.20 ms in the debug column is the log's own jitter: a line's first byte goes out at the end of that tick's pass, after the line has been queued and, on a scan tick, after the panel and pot have been read. Over a long run nothing accumulates: 16 steps took 1600 ms to within the same margin.

The very first step after reset is one tick short (99 ms in that run): the first tick begins step 0 and already counts towards step 1.

## The core — `core/`

```c
void seq_init(seq_state_t *p_state);
void seq_tick(seq_state_t *p_state, const seq_inputs_t *p_in, seq_outputs_t *p_out);

void    clock_init(clock_state_t *p_state);
uint8_t clock_advance(clock_state_t *p_state, uint16_t bpm, uint8_t elapsed_ticks);

void engine_plain_init(engine_plain_state_t *p_state);
bool engine_plain_step(engine_plain_state_t *p_state, const engine_inputs_t *p_in,
                       engine_step_t *p_step);

void address_init(address_state_t *p_state);
bool address_next(address_state_t *p_state, const address_config_t *p_config,
                  uint8_t *p_step);

bool    note_map_semitone(const note_map_config_t *p_config, uint8_t note,
                          uint8_t *p_semitone);
uint8_t panel_step_value(const uint8_t *p_raw_steps, uint8_t step);
```

`seq_tick()` reads the state and inputs, writes the state and outputs, and does nothing else. Every function under it is the same kind of function one level down, and each module has its own test program.

```mermaid
flowchart TD
    seq["seq<br/>when a step is due"] --> clock["clock<br/>ticks to pulses"]
    seq --> engine["engine_plain<br/>which step, and its note value"]
    seq --> note["note_map<br/>note value to pitch, or rest"]
    engine --> address["address<br/>next step in the range"]
    engine --> panel["panel<br/>a step's four switches"]
```

| Module | Question it answers | State it keeps |
|---|---|---|
| `seq` | Is a step due on this tick, and what are the outputs? | Tempo reading, pulses into the step, and the two below |
| `clock` | How many pulses fell in these ticks? | 2 bytes of phase |
| `engine_plain` | Which step plays now, and what is its note value? | The addressing state |
| `address` | Which step comes next in this direction and range? | A count of steps played in the cycle, and a finished flag |
| `note_map` | What pitch is this note value in this scale? | None |
| `panel` | What do the four switches of this step read? | None |

### The clock

The clock turns elapsed ticks into pulses, 96 to the quarter note, at a tempo in beats per minute. At B BPM there are B × 96 pulses in 60000 ticks, which is B × 8 pulses in 5000 ticks. So each tick adds B × 8 to a 16-bit phase counter, and each time the counter reaches 5000 one pulse is emitted and 5000 is taken off. Nothing is rounded, so the pulse count is exact however long it runs: the tests count exactly B × 96 pulses in a minute for a range of tempos, and no drift over ten minutes. Rounding the pulse period to whole ticks instead would be 4 % out at the fast end.

The tempo is limited to 300 BPM, which keeps the counter within 16 bits and means a tick never holds more than one pulse. A tempo of 0 stops the clock.

### The sequencer

- **Tempo.** The tempo reading (0 to 1023) maps linearly onto 30 to 285 BPM: 30 plus the reading divided by 4. The floor means the pot cannot stop the pattern or run it at an unusable speed. When a tempo read fails, the previous reading is reused, so the pattern keeps its pace. Before any valid reading the tempo is 30 BPM.
- **Step length.** A step is a sixteenth note: 24 pulses, or 15000 / BPM milliseconds. `step_pulses` counts the pulses since the current step began.
- **Step advance.** When `step_pulses` reaches 24 a step is due, and the engine is asked for it. If it has one, the outputs carry `b_step_started`, the step and what it plays. On every other tick `b_step_started` is false and the other outputs are zero. The first tick after `seq_init()` begins the first step of the pattern at once.
- **What a step plays.** Only the step that is beginning is read, from the inputs of that tick, so a change on the panel is heard the next time its step comes round. Its note value goes through the note mapping: 0 sets `b_rest`, anything else gives `semitone`. If the step inputs are flagged invalid, `b_note_valid` is false and the step is neither a rest nor a pitch; it still begins on time, so a failed read does not shift the pattern.
- **Reset.** While `b_reset` is true the engine is sent back to the start of its pattern on every tick. The clock is not touched, so the first step plays at the next step boundary, where the next step would have been anyway, and holding reset repeats the first step.
- **A finished pattern.** When a one-shot pattern has played, the clock keeps running and steps keep falling due, but none begins. A reset, or going back to looping, starts it again on the beat.
- **After a stall.** At most one step begins per tick. If the loop were held up for longer than a step, the steps owed would begin on successive ticks rather than being skipped.
- **State.** The clock's phase, the last valid tempo reading, the pulse count within the step, and the engine's state: 8 bytes.

```mermaid
flowchart TD
    start(["seq_tick"]) --> nullcheck{"Any pointer null?"}
    nullcheck -->|"yes"| done(["return, nothing written"])
    nullcheck -->|"no"| tempo{"b_tempo_valid?"}
    tempo -->|"yes"| store["last_tempo_raw = tempo_raw"]
    tempo -->|"no"| keep["keep the previous last_tempo_raw"]
    store --> clock
    keep --> clock
    clock["pulses = clock_advance(30 + last_tempo_raw / 4 BPM, elapsed_ticks)<br/>step_pulses += pulses"] --> due{"step_pulses at least 24?"}
    due -->|"no"| idle["b_step_started = false<br/>step, note = 0"]
    due -->|"yes"| ask["step_pulses -= 24<br/>engine_plain_step: is there a step?"]
    ask -->|"no: a one-shot pattern has finished"| idle
    ask -->|"yes"| begin["b_step_started = true<br/>step = the engine's step"]
    begin --> steps{"b_steps_valid?"}
    steps -->|"no"| unknown["b_note_valid = false<br/>neither rest nor pitch"]
    steps -->|"yes"| map{"note_map_semitone:<br/>note value 0?"}
    map -->|"yes"| rest["b_note_valid = true<br/>b_rest = true"]
    map -->|"no"| pitch["b_note_valid = true<br/>semitone = the pitch"]
    unknown --> out(["return"])
    rest --> out
    pitch --> out
    idle --> out
```

The chart leaves out reset: when `b_reset` is set, the engine is sent back to the start of its pattern before the clock is advanced, on every tick, and that has no output of its own.

### The plain engine and the engine interface

An engine decides what each step of the pattern is. The sequencer owns the clock and says when a step is due; the engine says which step it is and what note value it holds. Every engine is to have a state type, an init function that doubles as its reset, and a step function taking the same `engine_inputs_t` (the panel bits and the addressing controls) and filling in the same `engine_step_t` (a step number and a note value). `core/engine.h` holds those two types and describes the pattern.

`engine_plain` is the first and simplest: it asks the addressing for the next step and reads that step's four switches as the note value. Its state is the addressing state and nothing else. `seq.c` calls it directly; a selector between engines belongs with the second engine.

### Addressing

`address_next()` gives the step to play and moves on. The controls are a direction, a first and a last step, and a one-shot flag:

- **The range** is the steps from the first to the last. If the first is the higher of the two, the range runs through step 15 and round to step 0 (first 14, last 1 is steps 14, 15, 0, 1), so every pair of settings is a valid range of 1 to 16 steps.
- **A cycle** is one pass over the range: first to last going forward, last to first in reverse, and out and back as a pendulum with each end played once (0 1 2 3 2 1, then 0 again).
- **One-shot** stops after one cycle; `address_next()` then returns false until `address_init()` (the reset) or until one-shot is cleared.
- **The state** is how many steps of the cycle have been played, not a step number. That makes reset the same in every direction (the count goes to 0) and the end of a cycle one comparison. The cost is that a change of direction or range in the middle of a cycle carries the count over, so the pattern jumps to the matching point of the new cycle and does not turn round where it stands. A count beyond the end of a shortened cycle starts the cycle again.

Forward over the whole panel, which is what the firmware plays today, goes round like this:

```mermaid
stateDiagram-v2
    direction LR
    [*] --> Step0: seq_init, first tick
    Step0 --> Step1: 24 pulses
    Step1 --> Step2: 24 pulses
    Step2 --> Middle: 24 pulses
    Middle --> Step15: 24 pulses
    Step15 --> Step0: 24 pulses, wraps
    Step0: Step 0
    Step1: Step 1
    Step2: Step 2
    Middle: Steps 3 to 14
    Step15: Step 15
```

### The panel

`panel_step_value()` picks one step's four switches out of the raw bytes. The 64 bits are packed MSB first, 4 bits per step: step 0 is the high nibble of byte 0, step 1 the low nibble, step 2 the high nibble of byte 1, and so on. Every engine reads the same bits through this function and gives them its own meaning.

```mermaid
flowchart LR
    subgraph raw["raw_steps"]
        b0["byte 0"]
        b1["byte 1"]
        bn["bytes 2 to 6"]
        b7["byte 7"]
    end
    b0 -->|"bits 7..4"| s0["step 0"]
    b0 -->|"bits 3..0"| s1["step 1"]
    b1 -->|"bits 7..4"| s2["step 2"]
    b1 -->|"bits 3..0"| s3["step 3"]
    bn -->|"same pattern"| sn["steps 4 to 13"]
    b7 -->|"bits 7..4"| s14["step 14"]
    b7 -->|"bits 3..0"| s15["step 15"]
```

### Note mapping

`note_map_semitone()` turns a note value into a pitch. A value of 0 is a rest. Values 1 to 15 are the first fifteen notes of a scale, counting up from the root and carrying on into the next octaves: in a major scale 1 is the root, 8 the root an octave up, and 15 the root two octaves up. A pitch is a count of semitones above the lowest one the sequencer plays (note value 1 with a root of 0), from 0 to 45.

| Scale | Semitones above the root | Pitch of note values 1 to 15, root 0 |
|---|---|---|
| Chromatic | every one | 0 to 14 |
| Major | 0 2 4 5 7 9 11 | 0 to 24 |
| Natural minor | 0 2 3 5 7 8 10 | 0 to 24 |
| Major pentatonic | 0 2 4 7 9 | 0 to 33 |
| Minor pentatonic | 0 3 5 7 10 | 0 to 34 |

The root adds 0 to 11 semitones. There is no octave control yet: how many octaves are worth having depends on the DAC, which is not chosen. The scale tables are 51 bytes of constant data, which this toolchain keeps in RAM as well as flash; keeping them in flash only would take a compiler extension, which the core does not use.

## Target adapters — `adapters/target/`

### Register map — `atmega328p_regs.h` and `atmega328p_regs.ld`

Each register is declared as an ordinary variable, for example `extern volatile uint8_t PORTD;`, with no address in the C source. The linker places each name at its datasheet address using `atmega328p_regs.ld`, which the Makefile passes as an extra linker input. A register must appear in both files; one missing from the `.ld` file fails at link time with an undefined reference, but a wrong address there links silently.

The USART0 registers and their bit positions are declared in a second header, `atmega328p_usart_regs.h`, on the same scheme and with their addresses in the same `.ld` file. Only `serial_logger.c` includes it. The release build does not link that file, so with the bit positions in the main header they would be macros nothing in the release firmware uses (MISRA rule 2.5). The Timer/Counter2 registers have a header of their own in the same way, `atmega328p_timer2_regs.h`, which also names the interrupt vector and the attribute that makes a function its handler. The status register `SREG` is in the main header.

Registers in the low I/O range carry avr-gcc's `io_low` attribute, which lets the compiler keep using single-instruction bit operations (`sbi`, `cbi`, `sbis`) even though it cannot see the address. The generated code is identical to the earlier pointer-cast form. `ADC` is a 16-bit variable at 0x78, which reads ADCL then ADCH.

### Pin and ADC setup — `register_init.c`

| Pin | Nano label | Direction | Used for |
|---|---|---|---|
| PB5 | D13 (on-board LED) | output | Nothing yet; never written |
| PC0 | A0 | input | Nothing yet; never read |
| PC1 | A1 | output | Nothing yet; never written |
| PD2 | D2 | output | 74HC165 SH/LD (load, active low) |
| PD3 | D3 | output | 74HC165 CLK |
| PD4 | D4 | input | 74HC165 serial data (QH of nearest chip) |
| ADC6 | A6 | analog | Tempo pot |

The ADC is enabled with a prescaler of 128, giving a 125 kHz ADC clock from 16 MHz. A normal conversion takes 13 ADC clocks, about 104 µs.

### Tempo input — `analog_reader.c`

Writes `ADMUX` with AVcc as the reference and channel 6, sets `ADSC` to start a single conversion, polls until the hardware clears `ADSC`, then copies out the 10-bit result. The wait is bounded: after 10000 polls (a few milliseconds, against a worst-case conversion of about 200 µs) it returns `ERR_TIMEOUT`.

```mermaid
flowchart TD
    start(["tempo_input_read"]) --> check{"p_raw is null?"}
    check -->|"yes"| bad(["ERR_INVALID_PARAM<br/>nothing written"])
    check -->|"no"| mux["ADMUX: AVcc reference, channel 6"]
    mux --> go["Set ADSC to start one conversion"]
    go --> poll{"ADSC cleared<br/>by the hardware?"}
    poll -->|"yes"| read["Copy the 10-bit result to *p_raw"]
    read --> ok(["STATUS_OK"])
    poll -->|"no"| budget{"Polls left?<br/>10000 to start with"}
    budget -->|"yes"| poll
    budget -->|"no"| late(["ERR_TIMEOUT<br/>nothing written"])
```

### Step input — `shift_reg_reader.c`

Pulses PD2 low then high to latch all parallel inputs, then for each byte reads PD4 and pulses PD3 eight times. The bit is sampled before the clock pulse, which is correct for the 74HC165: the first bit is already on QH after the load. Bits are shifted in MSB first, which fixes the wiring-to-step mapping:

| | Bits 7..4 | Bits 3..0 |
|---|---|---|
| 74HC165 inputs | H G F E | D C B A |
| Chip `k` (0 = nearest the MCU) | step `2k` | step `2k + 1` |

Within each nibble the higher-lettered input is the more significant bit, so input H of the nearest chip is the MSB of step 0. Each chip's Clock Inhibit pin must be tied to GND. The clock and load pulses are a single `sbi`/`cbi` pair, about 125 ns wide at 16 MHz, comfortably above the 74HC165's minimum at 5 V.

The chain, with data moving right to left towards the MCU. Chip 0's bits arrive first and land in byte 0:

```mermaid
flowchart RL
    gnd["SER tied low"] --> c7
    subgraph chain["74HC165 chain: PD2 (load) and PD3 (clock) go to every chip"]
        c7["Chip 7<br/>steps 14 and 15"]
        mid["Chips 6 to 1<br/>steps 12 down to 2"]
        c0["Chip 0<br/>steps 0 and 1"]
    end
    c7 -->|"QH to SER"| mid
    mid -->|"QH to SER"| c0
    c0 -->|"QH"| pd4["PD4<br/>data in"]
```

One read, as the pulses the MCU sends and the bits it gets back:

```mermaid
sequenceDiagram
    participant M as MCU (shift_reg_reader.c)
    participant C as 74HC165 chain
    M->>C: PD2 low (latch all 64 inputs)
    M->>C: PD2 high (back to shift mode)
    Note over C: first bit is already on QH
    loop 8 bytes
        loop 8 bits, most significant first
            C-->>M: read PD4
            M->>C: PD3 high then low (shift to the next bit)
        end
        Note over M: store the byte in p_raw_bits
    end
```

### Log — `serial_logger.c` (debug build)

`logger_init()` sets the baud divisor from `F_CPU` for 115200 baud in double-speed mode (`16000000 / (8 × 115200) − 1 = 16.4`, rounded to the nearest divisor, 16), sets double speed and 8N1 explicitly, enables the transmitter and receiver, and empties the queue. Nothing reads from the UART.

A divisor of 16 gives 117647 baud, 2.1 % fast. That is the usual setting for 115200 on a 16 MHz AVR and what USB serial bridges are routinely run at, but it is not exact. Normal-speed mode cannot do better (its nearest divisor is 8.5 % off), which is why double speed is used. The rates that are exact at 16 MHz are 250000, 500000 and 1000000; changing `BAUD` in `serial_logger.c` is enough to switch, and the divisor follows from it.

Nothing in the logger waits for the UART. The port functions `log_step_note()`, `log_step_rest()` and `log_error()` own the message wording (`Step 3 Note = 11`, where the number is the pitch in semitones, and `Step 5 Rest`). Each checks the log level, then writes the message in pieces, fixed text through `write_text()` and numbers through `write_decimal()` (unpadded decimal digits), into a 128-byte ring queue. `log_poll()`, called on every pass of the main loop, sends one queued byte if the transmit buffer is empty and otherwise returns at once. The level is set to DEBUG at init, so step notes (DEBUG) and errors (ERROR) both print.

A message that does not fit in what is left of the queue is dropped whole: the bytes of it already queued are taken back out, so a partial line is never sent. The queue holds the two longest messages together (53 characters each). In normal running it never fills: the fastest tempo produces a 19-character line every 52.6 ms, and a line is gone in 1.6 ms.

`log_flush()` sends everything still queued and does wait. It is only used when initialization has failed and the main loop is not going to run.

```mermaid
flowchart TD
    call(["log_step_note or log_error"]) --> level{"Message level at or below<br/>the configured level?"}
    level -->|"no"| drop(["return, nothing queued"])
    level -->|"yes"| pieces["Write the message in pieces"]
    pieces --> text["write_text<br/>fixed wording"]
    pieces --> dec["write_decimal<br/>number as digits"]
    text --> ch["write_char"]
    dec --> ch
    ch --> room{"Room in the queue?"}
    room -->|"yes"| queue[("128-byte queue")]
    room -->|"no"| undo["Mark the message dropped;<br/>end_message takes its bytes back out"]
    poll(["log_poll, every pass of the main loop"]) --> any{"Anything queued<br/>and UDRE0 set?"}
    any -->|"no"| back(["return at once"])
    any -->|"yes"| udr["Move one byte from the queue to UDR0"]
    queue --> udr
    udr -->|"about 85 microseconds per byte"| wire(["PD1 to the USB serial chip"])
```

### Log — `null_logger.c` (release build)

The same six functions (`logger_init()`, `log_step_note()`, `log_step_rest()`, `log_error()`, `log_poll()`, `log_flush()`) with empty bodies. It touches no registers, so the USART is never set up or enabled, and it keeps no state. The app still calls the log port; each call returns immediately.

### Timebase — `timebase.c`

`timebase_init()` sets Timer/Counter2 to count the system clock divided by 64 (250 kHz) and to start again from zero after 250 counts (clear-on-compare mode, compare value 249), which is exactly 1000 times a second. It enables the compare match A interrupt, then sets the global interrupt enable bit in the status register. It is the last step of `platform_init()`, so interrupts are only on once everything else is set up.

The interrupt handler adds one to a single-byte counter, `g_tick_count`, and returns: 10 instructions including saving and restoring the one register it uses. It is the only code that runs outside the main loop, and it never touches the core or a port.

`timebase_elapsed_ticks()` reads the counter, subtracts what it read last time, and remembers the new value. The counter is one byte, so the read is a single instruction and cannot be interrupted half way; no masking is needed. Because the result is a difference, a pass that arrives late is told how many ticks it missed, up to 255, and none are lost. The counter wrapping from 255 to 0 does not disturb the subtraction.

```mermaid
sequenceDiagram
    participant T as Timer/Counter2
    participant I as Interrupt handler
    participant L as Main loop (app_run_once)
    Note over T: counts 0 to 249 at 250 kHz
    T->>I: compare match, every 1 ms
    I->>I: g_tick_count + 1
    L->>L: timebase_elapsed_ticks(): 0, spin
    L->>L: timebase_elapsed_ticks(): 1, run a tick
    T->>I: compare match
    I->>I: g_tick_count + 1
    L->>L: timebase_elapsed_ticks(): 1, run a tick
```

## Host side — `adapters/host/` and `tests/`

`adapters/host/host_ports.c` implements every port for the PC. Tests script what the input ports return (data and status) and read back a record of every output-port call in order. `make test` builds and runs fifteen programs with the PC compiler under `-std=c99 -pedantic -Wconversion -Wshadow -Werror`.

Seven link the code they test:

- `test_clock`: the clock alone. Exactly BPM × 96 pulses in a minute across the tempo range, no drift over ten minutes, the same count whether ticks arrive singly or in batches, the remainder carried between pulses, the 300 BPM limit, a tempo of 0, and null-pointer handling.
- `test_panel`: every step and nibble decodes from the right bits, a step number beyond the panel wraps, and a null buffer reads as 0.
- `test_address`: each direction over the whole panel and over a range, pendulum cycles of 30, 2 and 1 steps, a range that runs round through step 0, one-shot in every direction, reset, clearing and setting one-shot mid-cycle, a direction or range changed mid-cycle, out-of-range settings, and null-pointer handling.
- `test_note_map`: the rest, the pitch of all fifteen note values in each of the five scales, every root in every scale, the highest pitch, out-of-range notes, roots and scales, and null-pointer handling.
- `test_engine_plain`: steps in addressing order with their panel values, the panel read as it is when the step is asked for, a finished one-shot giving no step, reset, and null-pointer handling (the pattern does not move on).
- `test_seq`: the sequencer with the modules above linked in. The first tick begins step 0; step length for five tempos (exactly 15000 / BPM ticks, with the first step a tick short); 19 steps a second at the fastest setting; the steps in order with wrap; the pitch or rest of every step and nibble; the note taken from the tick its step begins on; a step beginning on time through a failed read; the tempo floor before any reading; last-valid-tempo reuse; independence of the two valid flags; batched ticks; steps owed after a stall; a rest; the scale and root applied; the addressing controls followed; a one-shot pattern stopping while the clock runs on; reset not moving the clock, held, and restarting a finished one-shot; and null-pointer handling.
- `test_app`: the real `app.c` and core linked against the fakes, with time scripted. Checks that a pass with no tick only polls the log; that a step is logged one tick after it begins, once; that steps are logged at the tempo and wrap; that a rest is logged as a rest; that all sixteen steps play forward in the chromatic scale; that the inputs are read on the first tick and every eighth after, counted in elapsed ticks; that a failed read logs the right error in the right place, once per step; and that init failure is logged, flushed and returned.

The other eight `#include` the one source file they test, with `tests/fake_atmega328p_regs.h` standing in for the register map (it claims the include guards of all three real register headers) and any function that file calls but does not define supplied as a stub by the test:

- `test_serial_logger`: the USART setup values (divisor 16, double speed, 8N1), the level filter, that nothing is sent until polled, one byte per poll, nothing while the transmit buffer is full, message order, a message that does not fit being dropped whole, the queue wrapping, `log_flush()`, and that every message is byte-for-byte what a `printf`-style format string produces.
- `test_null_logger`: init succeeds at any level, every message is discarded, polling and flushing do nothing, and no USART register is written.
- `test_register_init`: which direction bits are set and cleared, that the others and the output levels are left alone, and the ADC enable and prescaler value.
- `test_analog_reader`: channel and reference selection, the result, a slow conversion, the timeout at exactly the poll budget, and parameter checks. The fake ADC clears its start bit after a set number of polls.
- `test_shift_reg_reader`: every bit of the chain lands in the right place, one load pulse and eight clocks per byte, the lines left idle, other port D pins untouched, and parameter checks. The fake models the 74HC165 chain from the load and clock lines, so a read only returns the right bytes if the pulses come in the right order.
- `test_init`: logger bring-up, then registers, then the timebase; at DEBUG level; each failure returned, and the timebase (and so interrupts) not started after one.
- `test_timebase`: the Timer/Counter2 register values, that interrupts are enabled only after the timer is fully set up and without disturbing the other status bits, one tick per interrupt, ticks reported once, a late caller losing none up to 255, and the counter wrap. The fake header turns the interrupt handler into an ordinary function, which the test calls to make a tick happen.
- `test_main`: `main()` (renamed while included) returns the status when init fails and otherwise loops calling `app_run_once`. The stub leaves the endless loop with `longjmp`.

Which program tests which source file. Solid arrows link the real file; dotted arrows `#include` it against the fake registers. Each core program is linked with all of `core/`, so `test_seq` runs the real engine, addressing and note mapping under the sequencer:

```mermaid
flowchart LR
    t_seq["test_seq<br/>27 tests"] --> seq["core/seq.c"]
    t_clock["test_clock<br/>10 tests"] --> clock["core/clock.c"]
    t_engine["test_engine_plain<br/>6 tests"] --> engine["core/engine_plain.c"]
    t_address["test_address<br/>23 tests"] --> address["core/address.c"]
    t_note["test_note_map<br/>12 tests"] --> note["core/note_map.c"]
    t_panel["test_panel<br/>5 tests"] --> panel["core/panel.c"]
    t_app["test_app<br/>19 tests"] --> app["app/app.c"]
    t_app --> core["all of core/"]
    t_app --> hostp["adapters/host/host_ports.c"]
    t_main["test_main<br/>3 tests"] -.-> mainc["app/main.c"]
    t_ser["test_serial_logger<br/>18 tests"] -.-> ser["serial_logger.c"]
    t_null["test_null_logger<br/>3 tests"] -.-> nul["null_logger.c"]
    t_reg["test_register_init<br/>5 tests"] -.-> reg["register_init.c"]
    t_adc["test_analog_reader<br/>9 tests"] -.-> adc["analog_reader.c"]
    t_shift["test_shift_reg_reader<br/>8 tests"] -.-> shift["shift_reg_reader.c"]
    t_init["test_init<br/>4 tests"] -.-> init["init.c"]
    t_time["test_timebase<br/>9 tests"] -.-> time["timebase.c"]
```

```mermaid
pie showData
    title Host tests by area (161)
    "Core" : 83
    "Target adapters" : 56
    "App loop" : 19
    "main.c" : 3
```

### Coverage

`make coverage` rebuilds the same programs with `--coverage -O0` into `build/coverage/`, runs them, and has `tools/coverage/report.py` add up gcov's counts per source line across all the programs. It fails unless every `.c` file in `core/`, `app/` and `adapters/target/` has every line executed and every branch taken at least once; a firmware file no test compiles counts as a failure. `adapters/host/host_ports.c` is test support: its figures are printed (98% of lines, 75% of branches) but not enforced, and the tests themselves are not measured.

What this does not show: the fakes are plain variables, so nothing here checks a register's address, the `sbi`/`cbi` instructions the pulses rely on, that the interrupt vector reaches its handler, real timing, or what the hardware does in response. The emulated board below covers the first four; the last still needs the board.

## Emulated board — `tools/sim/`

`make sim` builds both configurations and runs each `output.hex`, exactly as it would be flashed, on an emulated ATmega328P. The emulator is the [avr8js](https://github.com/wokwi/avr8js) library (version pinned in `tools/sim/package-lock.json`), run under Node with its built-in test runner. Time is counted in CPU cycles at 16 MHz, so every run gives the same result.

| Folder | Holds | Knows about the sequencer |
|---|---|---|
| `lib/` | `machine.js`: loads the HEX file and builds the chip (CPU, ports B to D, ADC, USART0, Timer/Counter2) | No |
| `parts/` | `hc165.js`: a 74HC165 chain driven from its load, clock and data wires | No |
| `boards/` | `nano_sequencer.js`: which part is on which pin, the panel switches, the tempo pot, serial capture | Yes |
| `scenarios/` | `debug.test.js`, `release.test.js`: what each image must do | Yes |

The first two folders are kept free of anything specific to this project so they can be lifted out for another one.

Eleven scenarios run. For the debug image: every log line matches the switches set on the emulated panel across two passes of the pattern, as a pitch one below the note value or as a rest where all four switches are off; the USART is set to 115200 baud in double-speed mode; the panel is read every 8 ms with 64 clock pulses; a switch changed mid-pattern is heard the next time its step plays; a step lasts 100 ms at 150 BPM, with no drift over 16 steps; a step lasts 500 ms with the pot at zero; and the first step is logged within 3 ms of reset. For the release image: the USART is never enabled, nothing is sent and PD0 and PD1 are left as inputs; Timer/Counter2 holds the 1 kHz settings and interrupts are enabled; the panel is read every 8 ms, to within 0.01 ms, with 64 clock pulses; and the first read is on the first tick.

The release image has no log and the firmware has no other output yet, so nothing outside the chip shows when a step begins in that build. What the scenarios do show is that its tick runs at the same 1 kHz. Every object file but the logger is shared between the two builds, so the step timing code is the same machine code in both. A gate output will make it directly observable.

This is the first use of the emulator's timer and interrupt models. The figures it gave are in "The main loop and one tick" above.

What this does not show: the chip and the parts are models. The 74HC165 model follows the datasheet's logic (the load input is level-sensitive) but has no setup, hold or pulse-width limits, nothing analog is modelled, and only the GPIO, ADC, USART and Timer/Counter2 parts of avr8js have been exercised. The emulated clock is exact; the board's 16 MHz resonator is not, and one tick period should be compared against the board. `make sim` is not part of the coverage figure.

## Build — `Makefile`

`make` compiles every `.c` under `app/`, `core/` and `adapters/target/` (less the logger the configuration does not use) into `build/obj/`, links `build/<config>/output.elf`, converts to Intel HEX, and prints a size report. `make flash` uploads with avrdude at 57600 baud (the old-bootloader Nano setting), with the signature check and verification on. `make test`, `make coverage`, `make misra` and `make sim` are described above and in the README. `make check` runs both builds and those four through `tools/check/run.py`, which prints one line per stage and the full output only of a stage that fails. The Makefile handles Windows, macOS and Linux; only Windows has been exercised.

```mermaid
flowchart TD
    src["app/*.c<br/>core/*.c<br/>adapters/target/*.c"] -->|"avr-gcc -c, one call per file"| obj["build/obj/**/*.o<br/>shared by both configurations"]
    obj --> pick{"CONFIG"}
    pick -->|"debug: every object<br/>except null_logger.o"| dlink["Link"]
    pick -->|"release: every object<br/>except serial_logger.o"| rlink["Link"]
    ld["atmega328p_regs.ld<br/>register addresses"] --> dlink
    ld --> rlink
    crt["Startup object and libgcc<br/>added by the toolchain"] --> dlink
    crt --> rlink
    dlink --> delf["build/debug/output.elf"]
    rlink --> relf["build/release/output.elf"]
    delf -->|"avr-objcopy"| dhex["build/debug/output.hex"]
    relf -->|"avr-objcopy"| rhex["build/release/output.hex"]
    dhex -->|"make flash"| board["Board"]
    rhex -->|"make flash CONFIG=release"| board
```

### Build configurations

`CONFIG=debug` (the default) and `CONFIG=release` differ in one thing: which of `serial_logger.c` and `null_logger.c` is linked. There are no configuration macros and no conditional compilation; every source file is compiled the same way for both, so the two share `build/obj/`. Each has its own output folder (`build/debug/`, `build/release/`), so switching back and forth cannot leave a stale image. `make flash CONFIG=release` uploads the release image. `make test`, `make coverage` and `make misra` do not take a configuration: they always cover both loggers.

| | Debug | Release |
|---|---:|---:|
| Flash | 2534 bytes | 1800 bytes |
| Static RAM | 382 bytes | 94 bytes |
| Step timing | From the timer tick | The same |

A lower baud rate for release was considered and has no use: the release build never switches the USART on, so it has no baud rate.

### Where the flash goes

Sizes of the linked functions and data, from `avr-nm --size-sort` on each `output.elf`, grouped by source. The debug build:

```mermaid
pie showData
    title Debug build flash, 2534 bytes
    "Core code" : 858
    "Core scale tables" : 51
    "Serial logger code" : 556
    "Serial logger message text" : 155
    "App loop and main" : 392
    "Step and tempo input adapters" : 134
    "Vector table and startup" : 132
    "libgcc helpers" : 102
    "Timebase" : 90
    "Init adapters" : 64
```

| Part | Debug | Release |
|---|---:|---:|
| Core (code and scale tables) | 909 | 910 |
| Logger (code and message text) | 711 | 16 |
| App loop and `main` | 392 | 392 |
| Step and tempo input adapters | 134 | 134 |
| Vector table and startup code | 132 | 132 |
| libgcc helpers | 102 | 62 |
| Timebase (setup, interrupt handler, tick count) | 90 | 90 |
| Init adapters | 64 | 64 |
| **Total** | **2534** | **1800** |

Within the core: the sequencer 316 bytes, addressing 178, the plain engine 124, note mapping 106 plus its 51 bytes of tables (52 in the release build, with a byte of padding), the clock 84 and the panel 50.

The core is now the largest part of either image, and the serial logger is over a quarter of the debug one. The release build sheds one libgcc helper that only the logger needs, the 16-bit divide used to print decimal numbers. Both builds now carry the 8-bit divide (note mapping divides by the length of the scale) and the routine that copies initialised data into RAM at startup, which the release build did without until it had the scale tables. The debug build uses 8.2 % and the release build 5.9 % of the 30720 bytes available below the old bootloader.

Static RAM in the debug build is 382 bytes: the 128-byte log queue, 155 bytes of message text, the 51 bytes of scale tables, and 48 bytes of state. `avr-size` (and so `make` and `make check`) reports 176: this toolchain's `avr-size` counts constant data under flash only, although it is also copied to RAM. The release build is under-reported the same way now that it has the tables: 94 bytes used, 42 reported.

## Static analysis

`make misra` runs cppcheck with its MISRA addon three times, each time over the files that are linked together for that build: the debug firmware (`core/`, `ports/`, `app/`, `adapters/target/` without `null_logger.c`), the release firmware (the same without `serial_logger.c`) and the `test_app` host program (`core/`, `ports/`, `app/app.c`, `adapters/host/`, `tests/test_app.c`). Counts before the refactor are in [misra-baseline.md](misra-baseline.md): 165 findings, 5 of them mandatory and 101 required. There are now none. The source has one inline suppression, and it is a false positive rather than a real departure from the rule: rule 8.7 (advisory) on the timer interrupt handler in `timebase.c`. The handler is referenced from outside its file, by the jump in the startup object's vector table, so it must have external linkage; cppcheck analyses only the C source, never sees that reference, and reports the function as used in one file. It is recorded in the source in the deviation format so the reason sits beside the suppression.

Two things about scope: findings located in `tests/` are not reported, because test code is not held to the coding standard; the test file is in the host run only so cppcheck can see the host fakes being called. And cppcheck implements only part of MISRA C, so zero findings means zero from this tool, not a claim of full compliance. The `io_low` and `signal` attributes in the register headers are compiler extensions that the tool does not flag, and it did not object to the handler's reserved name (`__vector_7`).

How findings are handled is set out in [tools/misra/CLAUDE.md](../tools/misra/CLAUDE.md).

```mermaid
flowchart LR
    shared["core/, ports/"]
    shared --> d["misra-debug"]
    shared --> r["misra-release"]
    shared --> h["misra-host"]
    appall["app/app.c and app/main.c"] --> d
    appall --> r
    appc["app/app.c only"] --> h
    tgt["adapters/target/<br/>without the two loggers"] --> d
    tgt --> r
    ser["serial_logger.c"] --> d
    nul["null_logger.c"] --> r
    hst["adapters/host/<br/>tests/test_app.c"] --> h
    d --> zero(["0 findings each"])
    r --> zero
    h --> zero
```

## Open items

Earlier findings from this analysis that have since been fixed are in the git history (log level filter, flash port default, header dependency tracking, register map, UART setup, robustness gaps, stale comments, Makefile flags, coding-standard cleanup of the target adapters). What remains:

### 1. Unused pins are configured

PB5, PC0 and PC1 are given a direction in `register_init.c` but never read or written. PD0, the USART receive pin, used to be set as an output there as well; that line was removed, because the release build never enables the receiver and would have driven the pin against the board's USB serial chip. PD0 and PD1 are now left as they are at reset (inputs) unless the serial logger takes them over.

### 2. No crash log

Nothing records a reset or an error across power cycles, and the release build has no logging at all. A persistent error log in EEPROM is wanted and there is ample space, but it waits until there is a watchdog and an output stage, so that it can record real resets.

### 3. The timer tick has not run on the board

The 1 kHz tick, the interrupt and the non-blocking loop have been checked on the host and on the emulated board only. One period should be measured on the real board, both because the emulator's timer model is new to this project and because the board's resonator sets the real tempo accuracy.

### 4. The first step after reset is one tick short

The first tick begins step 0 and also counts towards step 1, so step 0 lasts 1 ms less than the others. It is harmless for a free-running pattern and will want fixing when there is a run/stop control and a step can begin on an external event.

### 5. `make check` under-reports static RAM

For the debug build it prints 176 bytes where 382 are used, and for the release build 42 where 94 are used, for the reason given under "Where the flash goes".

### 6. The pattern controls have no hardware

Direction, first and last step, one-shot, reset, scale and root are inputs to the core and are tested there, but nothing on the board sets them. `app.c` fixes them in `set_fixed_controls()`, so on the board (and the emulated one) only the forward, looping, chromatic path is ever taken, and nothing but the host tests exercises the rest. The roadmap gives them switches and pots in phases 6 and 7.

### 7. A change of direction or range mid-cycle jumps

The addressing counts steps played in the cycle, so reversing in the middle of a pattern jumps to the matching point of the reversed cycle; it does not turn round on the step it is on. Nothing can change these controls yet. If turning in place is wanted when they get hardware, it is a change inside `address.c` only.

Closed by the timebase work: the tempo range now has a floor (30 to 285 BPM), and the debug and release builds keep the same time, because the step period comes from the timer and not from a delay plus however long the rest of the loop took.

## What the output stage will need

- **Step advance**: done. The engine holds the position in the pattern; `seq_outputs_t` says when a step begins, which one, and its pitch or that it is a rest, and the app logs it.
- **A pitch to send and a rest to stay silent on**: done, as a semitone count from 0 to 45. Turning that into a DAC code is `pitch_cal`, still to come.
- **A usable tempo range**: done, 30 to 285 BPM.
- **A timing decision**: done. A 1 kHz timer tick, a loop that never blocks, and outputs applied at the tick boundary.
- **A new output port** (gate, trigger or CV) with a target adapter and a host fake, applied first in the tick's apply step. PB5, PC0 and PC1 are already configured and free.
- **New register definitions**, in a header per peripheral (and addresses in the `.ld` file), for whatever drives the output: SPI registers for an external DAC.
- **A gate length.** A step now has a duration in clock pulses (24), so a gate length can be a number of pulses within it; the core does not compute one yet.
