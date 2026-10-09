#!/usr/bin/env bash
# Downloads MISRA's official cppcheck headlines file into tools/misra/.
# The file is MISRA's copyrighted text, so it's gitignored. Run this once after cloning.
#
# Usage: tools/misra/fetch_misra_headlines.sh [--force]
# Override the source with: MISRA_HEADLINES_URL=<url> tools/misra/fetch_misra_headlines.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEST="${SCRIPT_DIR}/misra_c_2023__headlines_for_cppcheck.txt"

# Pinned to a known commit on MISRA's GitLab so the content doesn't change underneath us.
DEFAULT_URL="https://gitlab.com/MISRA/MISRA-C/MISRA-C-2012/tools/-/raw/ef4f0cd9300afc0e3f4d51da68692dd94e5ca5dc/misra_c_2023__headlines_for_cppcheck.txt"
URL="${MISRA_HEADLINES_URL:-$DEFAULT_URL}"

if [[ -f "$DEST" && "${1:-}" != "--force" ]]; then
    echo "Already present: $DEST (use --force to re-download)"
    exit 0
fi

TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT

echo "Downloading MISRA headlines from:"
echo "  $URL"

if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$URL" -o "$TMP"
elif command -v wget >/dev/null 2>&1; then
    wget -q "$URL" -O "$TMP"
else
    echo "error: need curl or wget" >&2
    exit 1
fi

# Sanity check: make sure we got the rules file and not an HTML error or login page.
if ! grep -q '^Rule [0-9]' "$TMP"; then
    echo "error: downloaded file doesn't look like a cppcheck MISRA headlines file." >&2
    echo "Check the URL, or download it manually from MISRA's GitLab into:" >&2
    echo "  $DEST" >&2
    exit 1
fi

mv "$TMP" "$DEST"
trap - EXIT
echo "Saved to $DEST"
