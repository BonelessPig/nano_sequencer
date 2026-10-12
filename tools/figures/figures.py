"""Measures the figures the documents quote and writes them to one file.

Usage: python tools/figures/figures.py <make program> <node program> <output file>

Writes, as Markdown:
  - flash and static RAM of each firmware image, in total and by source file,
    from the linker's map file (build/<config>/output.map);
  - the number of host tests, counted as RUN_TEST calls in tests/test_*.c;
  - line and branch coverage, from `make coverage`;
  - the number of emulated-board scenarios in each file, from running them.

The three images must be built before this runs; `make figures` sees to that.
Nothing here is dated, so running it again on unchanged source writes the same
file, and `git diff` on the output shows whether a figure has moved.
Exits non-zero, writing nothing, if a test or a scenario fails.
"""
import glob
import json
import os
import re
import subprocess
import sys

CONFIGS = ["debug", "release", "midi"]

# Flash left for the firmware below the old Nano bootloader, in bytes
FLASH_AVAILABLE = 30720

# Output sections that take flash, and those that take RAM. Initialised data
# is in both: it is stored in flash and copied to RAM at start-up.
FLASH_SECTIONS = [".text", ".data"]
RAM_SECTIONS = [".data", ".bss", ".noinit"]

# An input section's line in the map file: its name, address, size and the
# object it came from. A long name has the rest on a line of its own.
INPUT_SECTION = re.compile(r"^ (\S+)\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(\S.*)$")
INPUT_SECTION_NAME = re.compile(r"^ (\S+)$")
INPUT_SECTION_REST = re.compile(r"^\s+0x[0-9a-f]+\s+0x([0-9a-f]+)\s+(\S.*)$")
FILL = re.compile(r"^ \*fill\*\s+0x[0-9a-f]+\s+0x([0-9a-f]+)")
OUTPUT_SECTION = re.compile(r"^(\.\S+)\s")

OTHER_ROWS = ["startup object", "libgcc", "padding"]


def source_of(obj):
    """Returns the row an object file's bytes are counted under."""
    obj = obj.replace("\\", "/")
    if "libgcc.a(" in obj:
        return "libgcc"
    match = re.search(r"build/obj/(.+)\.o$", obj)
    if match:
        return match.group(1) + ".c"
    return "startup object"


def read_map(path):
    """Returns {output section: {row: bytes}} for one image's map file."""
    sizes = {}
    section = None
    pending = False
    started = False

    with open(path, encoding="utf-8", errors="replace") as map_file:
        for line in map_file:
            line = line.rstrip("\n")
            if not started:
                started = line.startswith("Linker script and memory map")
                continue

            output = OUTPUT_SECTION.match(line)
            if output:
                section = output.group(1)
                pending = False
                continue

            row = None
            match = INPUT_SECTION.match(line)
            rest = INPUT_SECTION_REST.match(line) if pending else None
            fill = FILL.match(line)
            if match:
                size, row = int(match.group(2), 16), source_of(match.group(3))
            elif rest:
                size, row = int(rest.group(1), 16), source_of(rest.group(2))
            elif fill:
                size, row = int(fill.group(1), 16), "padding"
            pending = INPUT_SECTION_NAME.match(line) is not None

            if row is not None and size > 0:
                by_row = sizes.setdefault(section, {})
                by_row[row] = by_row.get(row, 0) + size

    return sizes


def totals(sizes, sections):
    """Adds up {row: bytes} over the named output sections."""
    result = {}
    for section in sections:
        for row, size in sizes.get(section, {}).items():
            result[row] = result.get(row, 0) + size
    return result


def row_order(row):
    """Sorts source files by folder and name, with the toolchain's rows last."""
    if row in OTHER_ROWS:
        return (1, OTHER_ROWS.index(row), "")
    return (0, 0, row)


def size_table(title, per_config):
    """Returns the lines of a table of bytes by row, a column per image."""
    rows = sorted({row for sizes in per_config.values() for row in sizes}, key=row_order)
    lines = [f"## {title}", "", "| | Debug | Release | Midi |", "|---|---:|---:|---:|"]

    folders = sorted({os.path.dirname(row) for row in rows if row not in OTHER_ROWS})
    for folder in folders:
        cells = [
            sum(size for row, size in per_config[config].items() if os.path.dirname(row) == folder
                and row not in OTHER_ROWS)
            for config in CONFIGS
        ]
        lines.append(f"| **All of `{folder}/`** | " + " | ".join(f"**{cell}**" for cell in cells) + " |")
    for row in rows:
        name = row if row in OTHER_ROWS else f"`{row}`"
        cells = [str(per_config[config].get(row, 0)) for config in CONFIGS]
        lines.append(f"| {name} | " + " | ".join(cells) + " |")
    cells = [f"**{sum(per_config[config].values())}**" for config in CONFIGS]
    lines.append("| **Total** | " + " | ".join(cells) + " |")
    return lines + [""]


def image_table(flash, ram):
    """Returns the lines of the table of each image's totals."""
    lines = ["## Images", "", "| | Flash | Of the flash available | Static RAM |", "|---|---:|---:|---:|"]
    for config in CONFIGS:
        used = sum(flash[config].values())
        share = 100.0 * used / FLASH_AVAILABLE
        lines.append(f"| {config} | {used} bytes | {share:.1f}% | {sum(ram[config].values())} bytes |")
    lines += [
        "",
        f"The flash available is the {FLASH_AVAILABLE} bytes below the old Nano bootloader.",
        "Static RAM is initialised data, zeroed data and the one byte kept through a reset;",
        "`avr-size` prints less, because it counts initialised data under flash only.",
        "",
    ]
    return lines


def test_area(name):
    """Returns which part of the firmware a test program covers."""
    # The app loop's tests and the sequencer's are split by topic
    if name == "app" or name.startswith("app_"):
        return "App loop"
    if name == "main":
        return "`main.c`"
    if os.path.exists(f"core/{name}.c") or name.startswith("seq_"):
        return "Core"
    return "Target adapters"


def test_table():
    """Returns the lines of the tables of host tests, by area and by program."""
    programs = {}
    for path in sorted(glob.glob("tests/test_*.c")):
        with open(path, encoding="utf-8", errors="replace") as source:
            count = len(re.findall(r"\bRUN_TEST\(", source.read()))
        programs[os.path.basename(path)[len("test_"):-len(".c")]] = count

    areas = {}
    for name, count in programs.items():
        tests, files = areas.get(test_area(name), (0, 0))
        areas[test_area(name)] = (tests + count, files + 1)

    lines = ["## Host tests", "", "| Area | Programs | Tests |", "|---|---:|---:|"]
    for area in ["Core", "App loop", "Target adapters", "`main.c`"]:
        tests, files = areas.get(area, (0, 0))
        lines.append(f"| {area} | {files} | {tests} |")
    lines.append(f"| **Total** | **{len(programs)}** | **{sum(programs.values())}** |")
    lines += ["", "| Program | Tests |", "|---|---:|"]
    lines += [f"| `test_{name}` | {count} |" for name, count in programs.items()]
    return lines + [""]


def run(command):
    """Runs a command, returns what it printed, and exits if it failed."""
    result = subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        encoding="utf-8", errors="replace", check=False,
    )
    if result.returncode != 0:
        print(result.stdout.rstrip())
        sys.exit(f"figures: `{' '.join(command)}` failed")
    return result.stdout


def coverage_lines(make):
    """Returns the lines of the coverage section, from `make coverage`."""
    output = run([make, "--no-print-directory", "coverage"])
    total = [line for line in output.splitlines() if line.startswith("Firmware total")]
    if not total:
        sys.exit("figures: `make coverage` printed no total")
    return ["## Coverage", "", total[-1].strip() + ", in `core/`, `app/` and `adapters/target/`.", ""]


def scenario_lines(node):
    """Returns the lines of the table of emulated-board scenarios, by file."""
    reporter = "./tools/figures/scenario_counts.mjs"
    output = run([node, "--test", f"--test-reporter={reporter}", "tools/sim/scenarios/*.test.js"])
    report = json.loads(output.strip().splitlines()[-1])
    if report["failed"]:
        sys.exit(f"figures: {report['failed']} scenarios failed; run `make sim`")

    counts = report["counts"]
    lines = ["## Emulated board scenarios", "", "| File | Scenarios |", "|---|---:|"]
    lines += [f"| `{name}` | {counts[name]} |" for name in sorted(counts)]
    lines.append(f"| **Total** | **{sum(counts.values())}** |")
    return lines + [""]


def main():
    make, node, output_path = sys.argv[1:4]

    maps = {config: read_map(f"build/{config}/output.map") for config in CONFIGS}
    flash = {config: totals(maps[config], FLASH_SECTIONS) for config in CONFIGS}
    ram = {config: totals(maps[config], RAM_SECTIONS) for config in CONFIGS}

    lines = [
        "# Figures",
        "",
        "Written by `make figures`; do not edit it by hand.",
        "Run that again after a change to the firmware or its tests, and commit what it writes.",
        "",
    ]
    lines += image_table(flash, ram)
    lines += size_table("Flash by source file, in bytes", flash)
    lines += size_table("Static RAM by source file, in bytes", ram)
    lines += test_table()
    lines += coverage_lines(make)
    lines += scenario_lines(node)

    with open(output_path, "w", encoding="utf-8", newline="\n") as output:
        output.write("\n".join(lines))
    print(f"figures: wrote {output_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
