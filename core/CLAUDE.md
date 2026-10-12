# core/ rules

Pure sequencer logic.
The same source is compiled for the MCU and for a PC.

- Include only this folder's own headers and `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`.
  No port headers, no register headers, nothing from `adapters/` or `app/`.
- No `volatile`, no ISR code, no register access, no compiler extensions or builtins.
  The host build uses `-std=c99 -pedantic -Werror`, so an extension fails `make test`.
- A core function reads its state and inputs and writes its state and outputs, and has no other effect.
  State is passed in by pointer; the core holds no globals.
- Inputs arrive as plain values with a valid flag for anything that can fail to be read.
  The core decides what an invalid input means; it never sees a `port_status_t`.
- Every function, including each static helper, is covered at 100% of lines and branches by the test program for its module (`tests/test_<module>.c`), not only through `test_seq`.
- Keep this folder at zero MISRA findings with no deviations.
- A function goes in with its first caller, as a port function does: one that only a later piece of work will call is a rule 8.7 finding until then.

## Modules

- One module, one question: `seq` (is a step due), `clock` (ticks to pulses, at a tempo or following external edges), `transport` (is the pattern running, and from which clock), `debounce` (a control's settled level), `engine_*` (which step, its note value, and whether it is tied to the next), `address` (next step in the range), `note_map` (note value to pitch), `panel` (a step's switches, and the mode switches), `gate` (is the note still held), `prng` (the next random byte), `pitch_cal` (pitch to CV value), `cal` (is this calibration mode, and what is it setting), `cal_record` (are these stored bytes a calibration table), `errlog` (which crash log record is the newest, and what this start-up adds), `crc8` (the checksum of these bytes), `midi_out` (which MIDI bytes this tick sends), `pickup` (has the pot met this stored value), `bank` (what each slot's amount is, and which the pot is setting), `mod` (how far a signal and an amount move a value, and whether they make a chance come up).
  A new one gets its own `.c`, `.h` and test program; `docs/ROADMAP.md` section 4 lists the ones planned.
- A module includes only the modules below it.
  `seq` is the top of the sequencer, and nothing includes `seq.h` from inside `core/`.
  The app calls `seq`, and beside it `cal` and `cal_record`, which share only `pitch_cal.h` and `panel.h` with it; the sequencer knows nothing of calibration.
  `errlog` stands apart from both, called once at start-up, and shares only `crc8` with `cal_record`.
  `midi_out` stands beside `seq` too: the app calls it after `seq_tick()` with plain values taken from the sequencer's outputs, and it includes nothing else from the core.
  `bank` stands beside `seq` as well, with `pickup` under it: the app runs it ahead of the sequencer and hands the sequencer the amounts it uses as plain values.
  Neither includes anything of the sequencer's, so that another board can take them as they are.
- Every pointer parameter is checked for null, and a null means do nothing and return the harmless value (false, 0).
  The header says so.
- An out-of-range control never fails: it wraps or falls back to a default, and the header says which.
  The core has no error codes.
- Controls arrive in a `<module>_config_t` inside `seq_inputs_t` and are read when a step begins, like the panel.
- Constant tables are `static const` inside the function that uses them (rule 8.9), and cost RAM as well as flash on this target, since flash-only data needs a compiler extension.
- A macro that only tests or comments would use is a rule 2.5 finding, because findings in `tests/` are suppressed but uses there do not count.
  Write the number in the comment and let the test define its own.
- Arithmetic wider than 8 bits has to work where `int` is 16 bits (the MCU) and where it is 32 (the PC, with `-Wconversion`).
  Keep intermediate results within 16 bits, say why they fit, and cast each product or sum back to its type; do not cast a narrower composite expression to a wider type (rule 10.8), give the operand the type first.
- A saturating add on a byte written as `x > (uint8_t)(255U - y)` passes the host build and fails avr-gcc with a sign-compare error, because `int` is 16 bits there.
  Do the sum in a `uint16_t` and clamp it.
  The host build does not catch this kind of thing, so compile new core code with avr-gcc (`make`) before calling it done.

## Topic rules

The lessons about one part of the core are in `.claude/rules/`, each loaded when a file in its `paths:` list is read.
A new lesson about one of these goes in its file; one about the whole folder goes here.
Read the file yourself before writing a new file that belongs to its topic.

- `core-transport.md`: transport and the external clock (`transport.*`, `clock.*`, `debounce.*`, `seq.*`).
- `core-bank.md`: the amount bank (`bank.*`, `pickup.*`).
- `core-mod.md`: modulation (`mod.*`, `seq.*`).
- `core-midi-out.md`: MIDI out (`midi_out.*`).
- `core-cal.md`: calibration (`cal.*`, `cal_record.*`, `pitch_cal.*`).
- `core-errlog.md`: the crash log (`errlog.*`).
- `core-engines.md`: engines (`engine*.*`, `seq.*`).
- `core-seq.md`: the sequencer's outputs and gate (`seq.*`, `gate.*`).
