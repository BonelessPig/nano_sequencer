"""Lists the tracked files that have grown past the size the rules allow.

Usage: python tools/check/sizes.py

A file is read whole to be worked on, so a large one costs every piece of work
that touches it. The limits are in characters, and are the ones the folder
rules give: a source, test or tooling file is split by topic before it passes
SOURCE_LIMIT, and a rules file, which is loaded without being asked for,
before it passes RULES_LIMIT. The documents in docs/ and the README are read a
section at a time and have no limit.

Prints one line and always exits 0: this points at a file to split, and does
not fail a check over it. A file that is large for a reason goes in ALLOWED,
with the reason.
"""
import os
import subprocess
import sys

SOURCE_LIMIT = 25000
RULES_LIMIT = 6000

SOURCE_SUFFIXES = (".c", ".h", ".js", ".mjs", ".py", ".html", ".css", ".mk")
SOURCE_NAMES = ("Makefile",)
UNLIMITED_FOLDERS = ("docs/",)

# Files over their limit that are to stay as they are, and why
ALLOWED = {
    "tests/fake_atmega328p_regs.h": "one register map, read by name",
    "app/CLAUDE.md": "all about app.c, so a split would load with the same file",
}


def limit_of(path):
    """Returns the limit for a file in characters, or None if it has none."""
    name = os.path.basename(path)
    if path.startswith(UNLIMITED_FOLDERS):
        return None
    if name == "CLAUDE.md" or path.startswith(".claude/rules/"):
        return RULES_LIMIT
    if name in SOURCE_NAMES or name.endswith(SOURCE_SUFFIXES):
        return SOURCE_LIMIT
    return None


def main():
    listing = subprocess.run(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"],
        stdout=subprocess.PIPE, check=True,
    )
    paths = [p for p in listing.stdout.decode("utf-8").split("\0") if p]
    over = []
    for path in sorted(paths):
        limit = limit_of(path)
        if limit is None or path in ALLOWED or not os.path.isfile(path):
            continue
        with open(path, encoding="utf-8", errors="replace", newline="") as source:
            size = len(source.read())
        if size > limit:
            over.append(f"{path} ({size} of {limit})")

    sys.stdout.reconfigure(errors="replace")
    if over:
        print(f"{len(over)} over their limit: {', '.join(over)}")
    else:
        print("every file within its limit")
    return 0


if __name__ == "__main__":
    sys.exit(main())
