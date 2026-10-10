# app/ rules

The only code that knows about both the core and the ports.

- `app.c` runs one tick in three steps: gather inputs through the ports into one snapshot, call the core, apply the outputs through the ports. Keep decisions in the core; this file only moves data and reports failed reads.
- A port's status goes to the core as a valid flag, and to the log as a status code. The core never sees a `port_status_t`.
- No register access and no MCU headers here. `app.c` is also linked into the `test_app` host program against the fakes.
- `main.c` is init, then the endless loop. It is left out of `test_app` and of the `misra-host` run (the test has its own `main`), and is tested by `tests/test_main.c`, which includes it with `main` renamed.
- Both files must stay at 100% of lines and branches.
