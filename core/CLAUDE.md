# core/ rules

Pure sequencer logic. The same source is compiled for the MCU and for a PC.

- Include only this folder's own headers and `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`. No port headers, no register headers, nothing from `adapters/` or `app/`.
- No `volatile`, no ISR code, no register access, no compiler extensions or builtins. The host build uses `-std=c99 -pedantic -Werror`, so an extension fails `make test`.
- A core function reads its state and inputs and writes its state and outputs, and has no other effect. State is passed in by pointer; the core holds no globals.
- Inputs arrive as plain values with a valid flag for anything that can fail to be read. The core decides what an invalid input means; it never sees a `port_status_t`.
- Every function, including each static helper, is covered by `tests/test_seq.c` (or the test program for its module) at 100% of lines and branches.
- Keep this folder at zero MISRA findings with no deviations.
- New behaviour lands here first, with its tests, before any adapter or wiring for it.
