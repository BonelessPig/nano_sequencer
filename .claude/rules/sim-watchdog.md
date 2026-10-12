---
paths:
  - "tools/sim/scenarios/watchdog.test.js"
  - "tools/sim/boards/crash_record.js"
---

# tools/sim: the watchdog and the crash log

Loaded with the files below, on top of `tools/sim/CLAUDE.md`, whose rules apply to all of `tools/sim/`.

- The firmware never hangs by itself.
  A watchdog scenario stops the tick from outside with `stopTick()`, which clears the interrupt enable bit in `SREG`, and goes in `watchdog.test.js`, inside the loop over the three if it should hold for every image.
  Call it at a pin edge or a log line, which the loop makes: inside the tick interrupt it does nothing.
  The emulator's watchdog reset restarts the program and sets the reset flag and nothing else; `lib/machine.js` puts the USART back as well, and lists the reset's cycle.
  From its second reset on the emulator's watchdog asks twice, an instruction apart, which the list counts as one.
  Parts waiting on the chip's clock (a clock source, a bouncing switch) carry on through the reset.
  Do not read anything about the chip's registers after a reset from such a scenario.
  The RAM is another matter: the emulator's reset leaves it alone, as a real watchdog reset does, and the crash log scenarios rely on that for the breadcrumb.
  A fresh chip's RAM is zeros and the firmware never writes a place of 0, so a record with a place in it shows the byte was kept.
- After a watchdog reset give the firmware 25 ms before reading the crash log from the EEPROM: a record is five bytes at 3.6 ms each.
  On the debug image the first step's log line is up to a millisecond later than the rest, behind the start-up line, and several more when the crash log has a record to report; time a first step from the pins.
