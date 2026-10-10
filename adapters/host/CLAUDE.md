# adapters/host/ rules

The ports faked for the PC: tests script what the input ports return and read back a record of every output-port call, in order.

- `host_ports.c` implements every function in `ports/`. A new port function needs its fake here in the same change, or `test_app` will not link.
- No hardware access, no register headers.
- This code is in the `misra-host` analysis run, so it is held to the coding standard like firmware source.
- Its coverage is reported by `make coverage` but not enforced.
- Control functions for tests (scripting inputs, reading the record) are declared in `host_ports.h`, never in a port header.
