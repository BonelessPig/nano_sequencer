---
paths:
  - "**/*.c"
  - "**/*.h"
---

# The coding standard

Loaded with any C source or header.

Follow BARR-C:2018 plus the MISRA-style safety rules and line length limits listed under "Coding standard" in `README.md`; that list is the project's standard and applies to every change.
It is the same list as the "C coding standard" and "Line length" in the owner's global rules, so with those in context don't read the README for it.
Two things are particular to this project: the standard covers the firmware source (`core/`, `ports/`, `adapters/` and `app/`) and not `tests/` or `tools/sim/`, and the functions to document in a header are the port functions.
