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

- One module, one question: `seq` (is a step due), `clock` (ticks to pulses), `engine_*` (which step and its note value), `address` (next step in the range), `note_map` (note value to pitch), `panel` (a step's switches), `gate` (is the note still held). A new one gets its own `.c`, `.h` and test program; `docs/ROADMAP.md` section 4 lists the ones planned.
- A module includes only the modules below it. `seq` is the top and the only one the app calls; nothing includes `seq.h` from inside `core/`.
- Every pointer parameter is checked for null, and a null means do nothing and return the harmless value (false, 0). The header says so.
- An out-of-range control never fails: it wraps or falls back to a default, and the header says which. The core has no error codes.
- Controls arrive in a `<module>_config_t` inside `seq_inputs_t` and are read when a step begins, like the panel.
- Constant tables are `static const` inside the function that uses them (rule 8.9), and cost RAM as well as flash on this target, since flash-only data needs a compiler extension.
- A macro that only tests or comments would use is a rule 2.5 finding. Write the number in the comment and let the test define its own.
- Outputs that are levels (`b_gate`, `b_clock_out`) are given on every tick, as the level after that tick. Outputs that describe a step are given on the tick it begins and are zero otherwise.
- Musical lengths are counted in clock pulses, never ticks, so they scale with the tempo. When a step begins late, what it starts is counted from when it was due (`step_pulses` after the step's 24 are taken off), not from the tick that noticed.
- Every step that falls due calls `gate_begin()`, note or not: that is what ends a tied gate at a rest or at the end of a one-shot pattern. Look at the gate's level only after it.

## Engines

- `engine.h` is the interface: a state type, `engine_<name>_init()` (which is also the reset) and `engine_<name>_step()`, taking `engine_inputs_t` and filling in `engine_step_t`. A new engine follows `engine_plain`.
- The sequencer owns the clock and decides when a step is due. An engine decides which step it is and its note value, and never touches time or pitch.
- An engine reads the panel only through `panel_step_value()`.
- A step function that returns false must leave its state where it was.
- `seq.c` calls the plain engine directly. The selector between engines is a `switch` in `seq.c`, added with the second engine; no function-pointer table.
