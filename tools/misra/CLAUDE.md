# MISRA analysis: how it is run and how findings are handled

MISRA C is a secondary, automated check. BARR-C plus the rules in `README.md` is the standard we write to. cppcheck only implements part of MISRA, and the headlines file has only one-line rule summaries, so use judgment.

## Running it

- `make -k misra` runs all four analysis runs (`misra-debug`, `misra-release`, `misra-midi`, `misra-host`) and exits 0 only with zero findings. The exact cppcheck command lines are in the Makefile.
- Pipe the output through `python tools/misra/summarize.py` for counts by folder, category and rule.
- **cppcheck treats its inputs as one program.** The target and host adapters define the same port functions, and so do the two target loggers and the two target MIDI outputs, so there is one run per program that is actually linked. The first three are the debug, release and midi firmware, each leaving out the logger and the MIDI output its build does not link. The host run mirrors `test_app`: it leaves out `app/main.c` (the test has its own `main`) and includes `tests/test_app.c` so cppcheck can see the host fakes being called.
- Test code is outside the coding standard, so findings located in `tests/` are not reported. That is the one approved path-wide suppression.
- `misra.json` points the addon at the MISRA headlines file in this folder, so findings come out with readable rule text and a category.
- The headlines file is copyrighted MISRA text, so it's gitignored and never committed. If it's missing, run `tools/misra/fetch_misra_headlines.sh`. If that fails, tell me rather than writing rule text yourself.
- **cppcheck does not define `__cppcheck__` when `-D` is on its command line**, so that macro cannot be used to show it different code.

## Handling findings

- **Mandatory**: always fix, no deviations.
- **Required**: fix by default. If fixing would make the code worse or isn't possible (most likely in `adapters/target/`, for register access), add a deviation instead.
- **Advisory**: fix when it's cheap and clearly improves the code. Otherwise leave it and mention it in your summary.
- `core/` stays at zero findings with no deviations. `adapters/target/` gets more leeway, because hardware access legitimately needs things like `volatile`.
- If a headline is ambiguous and you're not sure what the rule requires, don't guess and don't make sweeping changes. Flag it to me with the rule number.
- To deviate, suppress inline and justify it on the same spot:

  ```c
  /* DEVIATION: MISRA <rule> (<category>) — <reason> */
  // cppcheck-suppress misra-c2012-<rule>
  ```

- cppcheck's rule ID prefix (`misra-c2012-` or `misra-c2023-`) depends on its version. Use whatever prefix the actual output shows in suppressions.
- The rules on suppressing, quoting MISRA text and reporting findings are under "MISRA text" in the root `CLAUDE.md`.

## Lessons

- A lesson about one folder's code is in that folder's `CLAUDE.md` (`core/`, `adapters/target/`): rules 2.5, 8.7, 8.9 and 17.3 have come up so far. Read it before fixing a finding there.
- Two file-static objects with the same name in different files of one run are a rule 5.9 (advisory) finding. The host run sees `app/app.c` and `adapters/host/host_ports.c` together, so their statics need different names.
- Zero findings is zero from this tool, not a claim of full compliance. The current status is in `docs/ARCHITECTURE.md`.
