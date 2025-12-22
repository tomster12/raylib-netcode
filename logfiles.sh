#!/usr/bin/env bash
set -euo pipefail

SRC_DIR="${1:-src}"

find "$SRC_DIR" -type f | sort | while read -r file; do
    echo
    echo "### ${file}"
    echo '```'
    cat "$file"
    echo '```'
done
