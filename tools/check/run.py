"""Runs every check that defines "done" and prints one line per stage.

Usage: python tools/check/run.py <make program>

Each stage is a `make` target. Its output is captured: a stage that passes
prints a single line, with one figure picked out of its output where there is
a useful one, and a stage that fails prints everything it wrote. All stages
run even if an earlier one fails, so one run shows everything that is wrong.
Exits 0 only if every stage passed.
"""
import re
import subprocess
import sys
import time

# (label, make arguments, pattern for the line worth showing or None)
STAGES = [
    ("build debug", ["CONFIG=debug", "all"], r"^\s*\d+\s+\d+\s+\d+\s+\d+"),
    ("build release", ["CONFIG=release", "all"], r"^\s*\d+\s+\d+\s+\d+\s+\d+"),
    ("test", ["test"], None),
    ("coverage", ["coverage"], r"^Firmware total"),
    ("misra", ["-k", "misra"], None),
    ("sim", ["sim"], r"\bpass \d+"),
]


def describe(label, output, pattern):
    """Returns the figure to show beside a passing stage, or an empty string."""
    if pattern is None:
        return ""
    lines = [line for line in output.splitlines() if re.search(pattern, line)]
    if not lines:
        return ""
    if label.startswith("build"):
        text, data, bss = (int(field) for field in lines[-1].split()[:3])
        return f"{text + data} bytes flash, {data + bss} bytes static RAM"
    if label == "sim":
        return f"{lines[-1].split()[-1]} scenarios passed"
    return lines[-1].strip()


def run_stage(make, label, arguments, pattern):
    """Runs one stage, prints its result, and returns True if it passed."""
    started = time.monotonic()
    result = subprocess.run(
        [make, "--no-print-directory"] + arguments,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        encoding="utf-8", errors="replace", check=False,
    )
    seconds = time.monotonic() - started
    passed = result.returncode == 0

    if passed:
        detail = describe(label, result.stdout, pattern)
        suffix = f"  {detail}" if detail else ""
        print(f"ok    {label:<14}{seconds:5.1f} s{suffix}", flush=True)
    else:
        print(f"FAIL  {label:<14}{seconds:5.1f} s", flush=True)
        print(result.stdout.rstrip(), flush=True)
        print(f"----  end of {label}", flush=True)

    return passed


def main():
    make = sys.argv[1]
    # A stage's output can hold characters the console cannot show
    sys.stdout.reconfigure(errors="replace")
    failed = [
        label for label, arguments, pattern in STAGES
        if not run_stage(make, label, arguments, pattern)
    ]

    if failed:
        print(f"check: FAILED ({', '.join(failed)})")
        return 1
    print("check: all passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
