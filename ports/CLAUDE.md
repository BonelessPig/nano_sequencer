# ports/ rules

Headers only: what the app needs from the outside world, one small header per need. The core owns these; adapters implement them.

- Name a port for what the application needs (`step_input_port.h`, `tempo_input_port.h`), not for the bus or chip that happens to provide it.
- Document every port function in its header: purpose, units, valid ranges, and whether it's ISR-safe.
- A port header includes nothing from `adapters/` and names no register, pin or MCU.
- Every port function has two implementations: one in `adapters/target/` and a fake in `adapters/host/host_ports.c`. `log_port.h` has two on the target, one per logger. Add all of them in the same change.
- A port function never waits for slow hardware. Where the work takes longer than a tick (the log, a write to non-volatile storage), the port has a function that only starts it and a `_poll()` function the main loop calls on every pass.
- An input that can come and go between two ticks (a clock edge, a reset pulse) is a port function that reports whether one has arrived since it was last called, and forgets it (`clock_input_edge()`). The app calls it once on every tick. A control that bounces (a button, a switch) is given as read; the core debounces it.
- Functions that can fail return `port_status_t` from `port_status.h`. Its values are printed in the serial log, so never renumber them; add new codes with new numbers.
