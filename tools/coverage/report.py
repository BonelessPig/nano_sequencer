#!/usr/bin/env python3
"""Report line and branch coverage of the host tests, and fail below 100%.

Usage:  python tools/coverage/report.py <gcov command> <coverage build folder>
        (run from the repository root; `make coverage` does this)

Reads every .gcno/.gcda pair the instrumented test programs left in the build
folder, by asking gcov for its JSON output, and adds the counts up per source
line. A file compiled into more than one test program (core/seq.c) is therefore
judged on all of its tests together.

Firmware source (core/, app/, adapters/target/) must be at 100% of lines and
100% of branches, each taken at least once; a firmware .c file that no test
compiled at all counts as a failure. The host fakes (adapters/host/) are test
support: their figures are printed but do not affect the result. Test code
itself is left out.
"""
import collections
import glob
import json
import os
import subprocess
import sys

FIRMWARE_DIRS = ["core", "app", "adapters/target"]
SUPPORT_DIRS = ["adapters/host"]


class FileCoverage:
    def __init__(self):
        self.lines = collections.Counter()     # line number -> times executed
        self.branches = collections.Counter()  # (line number, branch index) -> times taken

    def line_totals(self):
        return sum(1 for count in self.lines.values() if count > 0), len(self.lines)

    def branch_totals(self):
        return sum(1 for count in self.branches.values() if count > 0), len(self.branches)

    def is_complete(self):
        lines_hit, lines = self.line_totals()
        branches_hit, branches = self.branch_totals()
        return lines_hit == lines and branches_hit == branches


def relative_path(path, base, root):
    """Path of a source file relative to the repository root, with forward slashes."""
    full = os.path.normpath(os.path.join(base, path))
    return os.path.relpath(full, root).replace("\\", "/")


def folder_in(path, folders):
    return any(path.startswith(folder + "/") for folder in folders)


def collect(gcov, build_dir, root):
    """Run gcov over every notes file in build_dir and merge the counts per source file."""
    coverage = collections.defaultdict(FileCoverage)
    notes = sorted(glob.glob(os.path.join(build_dir, "*.gcno")))
    if not notes:
        sys.exit("No .gcno files in %s; was it built with --coverage?" % build_dir)

    for note in notes:
        result = subprocess.run(
            [gcov, "--json-format", "--stdout", "--branch-probabilities", note],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
        )
        if result.returncode != 0:
            sys.exit("gcov failed on %s:\n%s" % (note, result.stderr.decode(errors="replace")))
        report = json.loads(result.stdout.decode("utf-8", errors="replace"))
        base = report.get("current_working_directory", root)
        for entry in report["files"]:
            path = relative_path(entry["file"], base, root)
            if not folder_in(path, FIRMWARE_DIRS + SUPPORT_DIRS):
                continue
            target = coverage[path]
            for line in entry["lines"]:
                number = line["line_number"]
                target.lines[number] += line["count"]
                for index, branch in enumerate(line["branches"]):
                    target.branches[(number, index)] += branch["count"]
    return coverage


def percent(hit, total):
    return "%d/%d (%.1f%%)" % (hit, total, 100.0 * hit / total) if total else "none"


def print_table(title, paths, coverage):
    print(title)
    print()
    print("| File | Lines | Branches taken |")
    print("|---|---:|---:|")
    for path in paths:
        if path in coverage:
            print("| `%s` | %s | %s |" % (
                path, percent(*coverage[path].line_totals()), percent(*coverage[path].branch_totals())))
        else:
            print("| `%s` | not built by any test | |" % path)
    print()


def print_gaps(paths, coverage):
    for path in paths:
        if path not in coverage:
            continue
        data = coverage[path]
        for number in sorted(n for n, count in data.lines.items() if count == 0):
            print("  %s:%d: line never executed" % (path, number))
        per_line = collections.Counter(number for number, _ in data.branches)
        for number, index in sorted(k for k, count in data.branches.items() if count == 0):
            if data.lines.get(number, 0) > 0:
                print("  %s:%d: branch %d of %d never taken" % (path, number, index + 1, per_line[number]))


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    gcov, build_dir = sys.argv[1], sys.argv[2]
    root = os.getcwd()
    coverage = collect(gcov, build_dir, root)

    firmware = sorted(
        path.replace("\\", "/")
        for folder in FIRMWARE_DIRS
        for path in glob.glob(os.path.join(folder, "*.c"))
    )
    support = sorted(path for path in coverage if folder_in(path, SUPPORT_DIRS))

    print_table("Firmware (must be 100%)", firmware, coverage)
    if support:
        print_table("Test support (reported only)", support, coverage)

    incomplete = [path for path in firmware if path not in coverage or not coverage[path].is_complete()]
    if any(path not in coverage or not coverage[path].is_complete() for path in firmware + support):
        print("Not covered:")
        print_gaps(firmware + support, coverage)
        print()

    lines_hit = sum(coverage[path].line_totals()[0] for path in firmware if path in coverage)
    lines = sum(coverage[path].line_totals()[1] for path in firmware if path in coverage)
    branches_hit = sum(coverage[path].branch_totals()[0] for path in firmware if path in coverage)
    branches = sum(coverage[path].branch_totals()[1] for path in firmware if path in coverage)
    print("Firmware total: lines %s, branches %s" % (percent(lines_hit, lines), percent(branches_hit, branches)))

    if incomplete:
        sys.exit("FAILED: %d firmware file(s) below 100%%: %s" % (len(incomplete), ", ".join(incomplete)))
    print("OK: every firmware file is at 100% of lines and branches")


if __name__ == "__main__":
    main()
