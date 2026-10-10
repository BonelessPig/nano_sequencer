# ports/ rules

Headers only: what the app needs from the outside world, one small header per need. The core owns these; adapters implement them.

- Name a port for what the application needs (`step_input_port.h`, `tempo_input_port.h`), not for the bus or chip that happens to provide it.
- Document every port function in its header: purpose, units, valid ranges, and whether it's ISR-safe.
- A port header includes nothing from `adapters/` and names no register, pin or MCU.
- Every port function has two implementations: one in `adapters/target/` and a fake in `adapters/host/host_ports.c`. `log_port.h` has two on the target, one per logger. Add all of them in the same change.
- Functions that can fail return `port_status_t` from `port_status.h`. Its values are printed in the serial log, so never renumber them; add new codes with new numbers.
