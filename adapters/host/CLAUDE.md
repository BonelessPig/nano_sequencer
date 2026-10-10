# adapters/host/ rules

The ports faked for the PC: tests script what the input ports return and read back a record of every output-port call, in order.

- `host_ports.c` implements every function in `ports/`. A new port function needs its fake here in the same change, or `test_app` will not link.
- An output written on every tick (the gate, the clock output) is not put in the event record, which would fill in 64 ticks. The fake keeps its level and counts its rising edges; `host_event_count_at_gate_rise()` is what shows whether the gate or the log came first.
- Time is scripted too: `timebase_elapsed_ticks()` returns whatever the test last set, on every call, so a test moves time on by calling `app_run_once()` once per tick.
- No hardware access, no register headers.
- This code is in the `misra-host` analysis run, so it is held to the coding standard like firmware source.
- Its coverage is reported by `make coverage` but not enforced.
- Control functions for tests (scripting inputs, reading the record) are declared in `host_ports.h`, never in a port header.
