# Sequencer firmware: project rules

This is bare-metal C firmware for a microcontroller step sequencer. These rules apply to every change.

## Architecture: ports and adapters with a pure core

- `core/` is pure logic. It has no vendor/HAL headers, register access, `volatile`, or ISR code, and it must compile on a PC.
- The core talks to the outside world only through headers in `ports/`, which the core owns.
- `adapters/target/` implements the ports for the MCU, and `adapters/host/` implements them for PC builds and tests.
- `app/` (main and the scheduler loop) is the only place that wires adapters to the core.
- Ports are bound at link time. Don't add function-pointer indirection without a clear reason.
- Each tick runs in three steps: gather inputs, then the core step function, then apply outputs.
- ISRs do the minimum (counters, flags, ring buffers) and never call into the core.
- New features go into the core first, with tests. Hardware support goes into an adapter.

## Coding standard

Follow BARR-C:2018 (the Barr Group Embedded C Coding Standard) for style and naming. Also follow these MISRA-style safety rules:

- Use C99 and fixed-width types from `<stdint.h>` for anything with a meaningful size.
- Don't allocate dynamically (no malloc/free) and don't use recursion. All memory is static or on the stack, with bounded sizes.
- Every `switch` has a `default`, and there's no fallthrough without a comment.
- Every `if`/`else`/`for`/`while` body uses braces.
- Each function has a single, clear purpose. Keep functions short and nesting shallow.
- Don't use implicit conversions that lose data or change signedness. Cast explicitly and make sure the cast is justified.
- Check the return value of every function that can fail, or explicitly cast it to `(void)`.
- Use no magic numbers. Use named constants or enums.
- Make anything not used outside its file `static`. Minimize globals, and mark globals shared with an ISR `volatile` and access them atomically or with interrupts masked.
- Don't use the preprocessor for logic when a `static inline` function or an enum works.
- Document every port function in its header: purpose, units, valid ranges, and whether it's ISR-safe.

## Build and checks

- The target build must compile with `-Wall -Wextra -Wconversion -Wshadow -Werror` (or the toolchain equivalent).
- Host tests must pass before a change is considered done.
- Run cppcheck with the MISRA addon over `core/`, `ports/`, `adapters/` and `app/` before calling a change done. Use `make -k misra`, which runs it once per set of adapters (cppcheck treats its inputs as one program, and the target and host adapters define the same port functions):

  ```
  cppcheck --addon=tools/misra/misra.json --std=c99 --enable=warning,style,performance,portability --inline-suppr --error-exitcode=1 -DF_CPU=16000000UL -I core -I ports core ports app adapters/target
  cppcheck --addon=tools/misra/misra.json --std=c99 --enable=warning,style,performance,portability --inline-suppr --error-exitcode=1 -DF_CPU=16000000UL -I core -I ports core ports app adapters/host
  ```

  `tools/misra/misra.json` points the addon at the MISRA headlines file in `tools/misra/`, so findings come out with readable rule text and a category.

- The headlines file is copyrighted MISRA text, so it's gitignored and never committed. If it's missing, run `tools/misra/fetch_misra_headlines.sh`. If that fails, tell me rather than writing rule text yourself.
- Never copy MISRA headline text into source comments, commits or docs. Refer to rules by number only.
- cppcheck's rule ID prefix (`misra-c2012-` or `misra-c2023-`) depends on its version. Use whatever prefix the actual output shows in suppressions.

## Handling MISRA findings

MISRA C is a secondary, automated check. BARR-C plus the rules above is the standard we write to. cppcheck only implements part of MISRA, and the headlines file has only one-line rule summaries, so use judgment:

- **Mandatory**: always fix, no deviations.
- **Required**: fix by default. If fixing would make the code worse or isn't possible (common in `adapters/target/` for register access and vendor headers), add a deviation instead.
- **Advisory**: fix when it's cheap and clearly improves the code. Otherwise leave it and mention it in your summary.
- `core/` should be as close to zero findings as practical. `adapters/target/` gets more leeway, because hardware access legitimately needs things like casts to register addresses and `volatile`.
- If a headline is ambiguous and you're not sure what the rule requires, don't guess and don't make sweeping changes. Flag it to me with the rule number.
- To deviate, suppress inline and justify it on the same spot:

  ```c
  /* DEVIATION: MISRA <rule> (<category>) — <reason> */
  // cppcheck-suppress misra-c2012-<rule>
  ```

- Never suppress a rule for a whole file or the whole project without asking me first.
- When you finish a task, report new MISRA findings by rule and category, along with any deviations you added.

## Working style

- Make small, incremental changes, keep the build green after each one, and make one logical change per commit.
- Don't change behavior while refactoring. Report suspected bugs instead of silently fixing them.
