#!/usr/bin/env python3
"""Summarize cppcheck output as Markdown tables of finding counts.

Usage:  make misra 2>&1 | python tools/misra/summarize.py
        python tools/misra/summarize.py saved_output.txt

Counts findings by folder and MISRA category, and by rule. Categories are read
from the headlines file next to this script. Only rule numbers and categories
are printed, never the rule text, so the output is safe to commit.
"""
import collections
import glob
import os
import re
import sys

FINDING = re.compile(
    r"^(?P<file>[^:\n]+):(?P<line>\d+):(?P<col>\d+): "
    r"(?P<severity>error|warning|style|performance|portability): "
    r".*\[(?P<id>[^\]\s]+)\]\s*$"
)
MISRA_ID = re.compile(r"^misra-c\d+-(?P<rule>\d+\.\d+)$")
HEADLINE = re.compile(r"^Rule (?P<rule>\d+\.\d+)\s+(?P<category>\w+)\s*$")
CATEGORIES = ["Mandatory", "Required", "Advisory", "Unknown", "Not MISRA"]


def load_categories():
    """Map rule number -> category from the headlines file, if present."""
    here = os.path.dirname(os.path.abspath(__file__))
    categories = {}
    for path in glob.glob(os.path.join(here, "*headlines*.txt")):
        with open(path, encoding="utf-8", errors="replace") as handle:
            for text in handle:
                match = HEADLINE.match(text)
                if match:
                    categories[match.group("rule")] = match.group("category")
    return categories


def folder_of(path):
    """Group by folder, at most two levels deep (core, adapters/target); a top-level file is its own group."""
    parts = path.replace("\\", "/").split("/")
    return "/".join(parts[:-1][:2]) if len(parts) > 1 else path


def rule_key(rule):
    return [int(piece) if piece.isdigit() else 0 for piece in rule.split(".")]


def main():
    source = open(sys.argv[1], encoding="utf-8", errors="replace") if len(sys.argv) > 1 else sys.stdin
    categories = load_categories()

    seen = set()
    by_folder = collections.defaultdict(collections.Counter)
    by_rule = collections.Counter()
    for text in source:
        match = FINDING.match(text.rstrip("\r\n"))
        if not match:
            continue
        key = (match.group("file"), match.group("line"), match.group("col"), match.group("id"))
        if key in seen:
            continue
        seen.add(key)

        misra = MISRA_ID.match(match.group("id"))
        if misra:
            rule = misra.group("rule")
            category = categories.get(rule, "Unknown")
        else:
            rule = match.group("id")
            category = "Not MISRA"
        by_folder[folder_of(match.group("file"))][category] += 1
        by_rule[(rule, category)] += 1

    used = [c for c in CATEGORIES if any(counts[c] for counts in by_folder.values())]
    print("Total findings: %d" % len(seen))
    print()
    print("| Folder | " + " | ".join(used) + " | Total |")
    print("|---|" + "---:|" * (len(used) + 1))
    for folder in sorted(by_folder):
        counts = by_folder[folder]
        cells = " | ".join(str(counts[c]) for c in used)
        print("| `%s` | %s | %d |" % (folder, cells, sum(counts.values())))
    totals = " | ".join(str(sum(counts[c] for counts in by_folder.values())) for c in used)
    print("| **All** | %s | %d |" % (totals, len(seen)))
    print()
    print("| Rule | Category | Count |")
    print("|---|---|---:|")
    misra_rows = sorted((k for k in by_rule if k[1] != "Not MISRA"), key=lambda k: rule_key(k[0]))
    other_rows = sorted(k for k in by_rule if k[1] == "Not MISRA")
    for rule, category in misra_rows + other_rows:
        print("| %s | %s | %d |" % (rule, category, by_rule[(rule, category)]))


if __name__ == "__main__":
    main()
