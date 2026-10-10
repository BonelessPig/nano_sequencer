# MISRA analysis: how it is run and how findings are handled

MISRA C is a secondary, automated check. BARR-C plus the rules in `README.md` is the standard we write to. cppcheck only implements part of MISRA, and the headlines file has only one-line rule summaries, so use judgment.

## Running it

- `make -k misra` runs all three analysis runs (`misra-debug`, `misra-release`, `misra-host`) and exits 0 only with zero findings. The exact cppcheck command lines are in the Makefile.
- Pipe the output through `python tools/misra/summarize.py` for counts by folder, category and rule.
- **cppcheck treats its inputs as one program.** The target and host adapters define the same port functions, and so do the two target loggers, so there is one run per program that is actually linked. The first two are the debug and release firmware. The host run mirrors `test_app`: it leaves out `app/main.c` (the test has its own `main`) and includes `tests/test_app.c` so cppcheck can see the host fakes being called.
- Test code is outside the coding standard, so findings located in `tests/` are not reported. That is the one approved path-wide suppression.
- `misra.json` points the addon at the MISRA headlines file in this folder, so findings come out with readable rule text and a category.
- The headlines file is copyrighted MISRA text, so it's gitignored and never committed. If it's missing, run `tools/misra/fetch_misra_headlines.sh`. If that fails, tell me rather than writing rule text yourself.
- **cppcheck does not define `__cppcheck__` when `-D` is on its command line**, so that macro cannot be used to show it different code.

## Handling findings

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

- cppcheck's rule ID prefix (`misra-c2012-` or `misra-c2023-`) depends on its version. Use whatever prefix the actual output shows in suppressions.
- Never suppress a rule for a whole file or the whole project without asking me first.
- Never copy MISRA headline text into source comments, commits or docs. Refer to rules by number only.
- When you finish a task, report new MISRA findings by rule and category, along with any deviations you added.

## Lessons

- A macro used by only one build configuration is a rule 2.5 finding in the other run. Put it in a header only that configuration's files include.
- A compiler builtin needs a visible prototype or it is a rule 17.3 finding.
- An interrupt handler is reported under rule 8.7 (advisory), and that is a false positive: the vector table in the startup object refers to it, which is why it needs external linkage, but cppcheck sees only the C source. It is suppressed inline in `adapters/target/timebase.c`, in the deviation format, with the reason. Do the same for each new handler.
- Two file-static objects with the same name in different files of one run are a rule 5.9 (advisory) finding. The host run sees `app/app.c` and `adapters/host/host_ports.c` together, so their statics need different names.
- A file-scope constant table used by one function is a rule 8.9 (advisory) finding; declare it `static const` inside the function.
- A macro in a core header that only a test or a comment uses is a rule 2.5 (advisory) finding in every run, because findings in `tests/` are suppressed but uses there do not count. Leave the macro out and let the test define its own.
- The current result is zero findings, with one inline suppression (the rule 8.7 false positive, above) and no true deviations. That is zero from this tool, not a claim of full compliance.
