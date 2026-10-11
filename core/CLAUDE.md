# core/ rules

Pure sequencer logic. The same source is compiled for the MCU and for a PC.

- Include only this folder's own headers and `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`. No port headers, no register headers, nothing from `adapters/` or `app/`.
- No `volatile`, no ISR code, no register access, no compiler extensions or builtins. The host build uses `-std=c99 -pedantic -Werror`, so an extension fails `make test`.
- A core function reads its state and inputs and writes its state and outputs, and has no other effect. State is passed in by pointer; the core holds no globals.
- Inputs arrive as plain values with a valid flag for anything that can fail to be read. The core decides what an invalid input means; it never sees a `port_status_t`.
- Every function, including each static helper, is covered at 100% of lines and branches by the test program for its module (`tests/test_<module>.c`), not only through `test_seq`.
- Keep this folder at zero MISRA findings with no deviations.
- New behaviour lands here first, with its tests, before any adapter or wiring for it.

## Modules

- One module, one question: `seq` (is a step due), `clock` (ticks to pulses, at a tempo or following external edges), `transport` (is the pattern running, and from which clock), `debounce` (a control's settled level), `engine_*` (which step, its note value, and whether it is tied to the next), `address` (next step in the range), `note_map` (note value to pitch), `panel` (a step's switches, and the mode switches), `gate` (is the note still held), `prng` (the next random byte), `pitch_cal` (pitch to CV value), `cal` (is this calibration mode, and what is it setting), `cal_record` (are these stored bytes a calibration table), `errlog` (which crash log record is the newest, and what this start-up adds), `crc8` (the checksum of these bytes). A new one gets its own `.c`, `.h` and test program; `docs/ROADMAP.md` section 4 lists the ones planned.
- A module includes only the modules below it. `seq` is the top of the sequencer, and nothing includes `seq.h` from inside `core/`. The app calls `seq`, and beside it `cal` and `cal_record`, which share only `pitch_cal.h` and `panel.h` with it; the sequencer knows nothing of calibration. `errlog` stands apart from both, called once at start-up, and shares only `crc8` with `cal_record`.
- Every pointer parameter is checked for null, and a null means do nothing and return the harmless value (false, 0). The header says so.
- An out-of-range control never fails: it wraps or falls back to a default, and the header says which. The core has no error codes.
- Controls arrive in a `<module>_config_t` inside `seq_inputs_t` and are read when a step begins, like the panel.
- Constant tables are `static const` inside the function that uses them (rule 8.9), and cost RAM as well as flash on this target, since flash-only data needs a compiler extension.
- A macro that only tests or comments would use is a rule 2.5 finding. Write the number in the comment and let the test define its own.
- The step lights (`leds`) are the lights of the step that last began, given while the clock output is high and 0 otherwise. An engine with more to show than its step (the Klee register) has it read from its state in `step_leds()` in `seq.c`; do not add a field to `engine_step_t` for one engine.
- Outputs that are levels (`b_gate`, `b_clock_out`, `cv`, `leds`, `transport`) are given on every tick, as the level after that tick. Outputs that describe a step are given on the tick it begins and are zero otherwise.
- Musical lengths are counted in clock pulses, never ticks, so they scale with the tempo. When a step begins late, what it starts is counted from when it was due (`step_pulses` after the step's 24 are taken off), not from the tick that noticed.
- The pitch CV value changes only when a note begins, on the tick its gate opens, and holds through everything else (rests, unreadable steps, reset, a finished one-shot, a stop and a start). The core gives it as a number from 0 to 4095 and does not know what that is in volts.
- Arithmetic wider than 8 bits has to work where `int` is 16 bits (the MCU) and where it is 32 (the PC, with `-Wconversion`). Keep intermediate results within 16 bits, say why they fit, and cast each product or sum back to its type; do not cast a narrower composite expression to a wider type (rule 10.8), give the operand the type first.
- Every step that falls due calls `gate_begin()`, note or not: that is what ends a tied gate at a rest or at the end of a one-shot pattern. Look at the gate's level only after it.

## Transport and the external clock

- `seq_tick()` runs the transport first, on every tick. A start (the first tick, and each time the run button starts the pattern) puts the clock, the engine and the gate back to their beginning and passes on no elapsed time for that tick, so the first step is a whole one. Do not let a start count its own tick.
- Stopped means nothing is due: the gate is closed on every stopped tick, the clock output is low, the pitch CV holds, and reset and the external clock are not looked at.
- A control that bounces (the run button, the sync switch) arrives as read, and `transport` debounces it. `debounce` counts in ticks, like `cal`: it is for a finger, not for music. Its first reading is taken as it stands; `transport` starts with the button as if held, so a button down at power-on is not a press.
- An input that is an event (a clock edge, a reset edge) arrives as a flag that is true for one tick. The core never sees how long the pulse was.
- The tempo reading has two meanings, and `seq.c` owns both: a tempo on the internal clock, a ratio on the external one. The ratio's parts are constants at the top of `seq.c`.
- `clock_follow()` keeps an exact count: 24 times the multiply from one sync edge to the next, never more and never fewer, whatever the edges do. It holds one pulse back for the edge, so a step never begins ahead of its edge, and gives all that is left when the edge comes. Keep that property; it is what stops the pattern slipping against the clock. Steps owed after an early edge begin one per tick, like steps owed after a stall.
- The ratio is read at a sync edge and held until the next, like a control read when a step begins. A multiply needs a measured spacing, so the first stretch is always given as one step.
- `clock_advance()` ends any following, so `seq.c` needs no state of its own for a change of clock. `clock_follow()` is told how many pulses the sequencer has left before its next step, and its first sync edge gives exactly that, so the step begins on the edge. A start on the external clock leaves the step one pulse short of due for the same reason.
- A change of clock is not a start and must not restart the pattern.
- On the external clock a step can fall due while the clock output is still high from the step before (an edge in under half the last gap). It is held back one tick, with the gate closed and the clock output low, so that every step has a rising edge of its own. The internal clock is left alone: there it takes a stalled loop, and a late step begins at once, as before. A gate tied by its length is not broken by this, because the clock output has fallen by then.

## Calibration

- `cal_tick()` runs on every tick, ahead of the sequencer. It chooses the mode once, on the first tick after `cal_init()`, and never changes its mind; outside the mode every output is false or 0 and the table is not touched.
- The mode's controls (the entry pattern, which step chooses the point, which switch stores, the trim range, the gate's low time) are constants at the top of `cal.c`. The header describes them in words, with the numbers, and the tests define their own.
- `cal` counts in ticks, not pulses: it has no clock, and its one time (the gate low after a store) is for a person, not for music.
- The core still does not know what a CV value is in volts: the nominal value of an octave is an input.
- A change to the layout of the record takes a new version number in `cal_record.c`, so that old records are refused and not misread. The checksum has a test vector worked out outside the firmware; keep one.

## The crash log

- `errlog` works on a copy of the stored bytes and never knows where they are kept. It changes at most one record and says which; the app writes it.
- It decides what a crash is, from the reset cause as a plain number: the watchdog's bit (8). The app does not test the cause.
- The place is the app's number and is stored as given; the core gives it no meaning.
- There is no pointer to the newest record and no count of them: the records' own numbers say. Keep it so, or there are two things to keep in step across a power cut.
- A repeat of the newest crash raises its count, and at 255 changes nothing. That is the wear limit; do not let a path past it write.
- A change to the layout of the record takes a new version number in `errlog.c`, which is the start value of its checksum, so old records read as empty places. The test vectors were worked out outside the firmware; keep them so.

## Engines

- `engine.h` is the interface: a state type, `engine_<name>_init()` (which is also the reset) and `engine_<name>_step()`, taking `engine_inputs_t` and filling in `engine_step_t`. A new engine follows `engine_plain`.
- The sequencer owns the clock and decides when a step is due. An engine decides which step it is and its note value, and never touches time or pitch.
- An engine reads the panel only through `panel_step_value()`, and its own mode switches (3 to 7) through `panel_mode_value()`. The low three are the sequencer's; an engine never looks at them, and ignores any of the five it has no use for.
- The engines are numbered 0 to 5: plain, stage, Klee, chance, Cartesian, Euclidean. 6 and 7 are free.
- Chance comes from the one generator in the sequencer's state, through `p_prng` in `engine_inputs_t`. An engine draws with `prng_byte()` and compares so that a draw of 0 means "leave it alone": a null generator gives 0, and then nothing is left to chance. Engine init never seeds it; only `seq_init()` does.
- A test of anything random uses the real generator from a seed whose first bytes are written in `test_prng.c`, and says in a comment which draws it expects. Do not fake the generator.
- An engine with no use for the addressing controls (Klee, Cartesian) does not check that pointer; its header says so.
- A step function that returns false must leave its state where it was.
- The selector between engines is a `switch` in `seq.c` on the low three mode switches as a number (`panel_mode_value()`); no function-pointer table. A number with no engine plays the plain one. A new engine takes the next number, a state of its own in `seq_state_t` beside the others (no union), a line in `engines_init()` and a case in `engine_step()`.
- The engine is chosen when a step begins, from the panel as last read, valid or not. A change of engine sends every engine back to its start, and so do a reset and a start.
- An engine is asked once for every step, and a step is always 24 pulses. An engine that makes something longer than a step (a stage of several steps) counts the steps itself and gives each one in turn: a silent one as a rest (note value 0), a held one as a note with `b_tie` set. It never asks the sequencer for a length.
- `b_tie` means the gate is still open when the next step begins. `seq.c` does that by giving the note a gate of a whole step if the gate length is shorter; the gate module knows nothing of ties. The stage engine leaves it off on the last step of a hold; the Klee engine's merge sets it on every note and lets the rest that follows end the gate at the boundary.
- An engine with fewer than sixteen places (the stage engine's eight) takes the addressing range modulo its own count and hands `address_next()` a range that does not wrap, then folds the step it gets back. Direction, pendulum and one-shot then need no code of their own.
- The stage engine's layout (which steps are controls, which switches are the length and which the mode, the order of the modes) is constants at the top of `engine_stage.c`, described in words in its header; the tests define their own.
