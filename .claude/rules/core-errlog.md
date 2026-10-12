---
paths:
  - "core/errlog.*"
---

# core: the crash log

Loaded with the files below, on top of `core/CLAUDE.md`, whose rules apply to all of `core/`.

- `errlog` works on a copy of the stored bytes and never knows where they are kept.
  It changes at most one record and says which; the app writes it.
- It decides what a crash is, from the reset cause as a plain number: the watchdog's bit (8).
  The app does not test the cause.
- The place is the app's number and is stored as given; the core gives it no meaning.
- There is no pointer to the newest record and no count of them: the records' own numbers say.
  Keep it so, or there are two things to keep in step across a power cut.
- A repeat of the newest crash raises its count, and at 255 changes nothing.
  That is the wear limit; do not let a path past it write.
- A change to the layout of the record takes a new version number in `errlog.c`, which is the start value of its checksum, so old records read as empty places.
  The test vectors were worked out outside the firmware; keep them so.
