"""Runs every check that defines "done" and prints one line per stage.

Usage: python tools/check/run.py <make program> [stage ...]

With no stage named, every stage runs. Naming stages by their labels runs only
those, which is how `make test-quiet` runs the host tests alone.

Each stage is a `make` target. Its output is captured: a stage that passes
prints a single line, with one figure picked out of its output where there is
a useful one, and a stage that fails prints everything it wrote. All stages
run even if an earlier one fails, so one run shows everything that is wrong.
Exits 0 only if every stage passed.

The builds and the host tests run one after another, since the builds share
their objects. Coverage, the analysis and the emulated board then run side by
side: each takes about as long as the other two and none writes where another
reads. Their lines are still printed in the order of the list.

The last stage, the file sizes, never fails: its line names the files that
have grown past the size the rules allow, to be split by topic.
"""
import concurrent.futures
import re
import subprocess
import sys
import time

# (label, make arguments, pattern for the line worth showing or None)
STAGES = [
    ("build debug", ["CONFIG=debug", "all"], r"^\s*\d+\s+\d+\s+\d+\s+\d+"),
    ("build release", ["CONFIG=release", "all"], r"^\s*\d+\s+\d+\s+\d+\s+\d+"),
    ("build midi", ["CONFIG=midi", "all"], r"^\s*\d+\s+\d+\s+\d+\s+\d+"),
    ("test", ["test"], r"^\d+ checks, \d+ failed"),
    ("coverage", ["coverage"], r"^Firmware total"),
    ("misra", ["-k", "misra"], None),
    ("sim", ["sim"], r"\bpass \d+"),
    ("sizes", ["file-sizes"], r"limit"),
]

# The stages that run side by side, after the others
SIDE_BY_SIDE = ("coverage", "misra", "sim")


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
    if label == "test":
        checks = sum(int(line.split()[0]) for line in lines)
        return f"{len(lines)} programs, {checks} checks"
    if label == "sim":
        return f"{lines[-1].split()[-1]} scenarios passed"
    return lines[-1].strip()


def run_stage(make, arguments):
    """Runs one stage and returns its result and how many seconds it took."""
    started = time.monotonic()
    result = subprocess.run(
        [make, "--no-print-directory"] + arguments,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        encoding="utf-8", errors="replace", check=False,
    )
    return result, time.monotonic() - started


def report_stage(label, pattern, result, seconds):
    """Prints one stage's result and returns True if it passed."""
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
    wanted = sys.argv[2:]
    known = [stage[0] for stage in STAGES]
    unknown = [label for label in wanted if label not in known]
    if unknown:
        print(f"check: no such stage ({', '.join(unknown)})")
        return 1
    stages = [stage for stage in STAGES if not wanted or stage[0] in wanted]
    failed = []

    with concurrent.futures.ThreadPoolExecutor(len(SIDE_BY_SIDE)) as pool:
        for label, arguments, pattern in stages:
            if label not in SIDE_BY_SIDE:
                result, seconds = run_stage(make, arguments)
                if not report_stage(label, pattern, result, seconds):
                    failed.append(label)
        running = [
            (label, pattern, pool.submit(run_stage, make, arguments))
            for label, arguments, pattern in stages if label in SIDE_BY_SIDE
        ]
        for label, pattern, future in running:
            result, seconds = future.result()
            if not report_stage(label, pattern, result, seconds):
                failed.append(label)

    if failed:
        print(f"check: FAILED ({', '.join(failed)})")
        return 1
    if not wanted:
        print("check: all passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
