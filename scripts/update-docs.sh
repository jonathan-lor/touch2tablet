#!/bin/sh
# Rewrite the table in docs/supported-panels.md from panels.json (tst_panels checks it is current).
set -eu
cd "$(dirname "$0")/.."
cmake -S . -B build -G Ninja >/dev/null
cmake --build build --target touch2tabletd
doc=docs/supported-panels.md
TABLE=$(build/daemon/touch2tabletd --print-panel-table) awk '
  /<!-- panels:end -->/ { skip = 0 }
  !skip
  /<!-- panels:begin -->/ { print ENVIRON["TABLE"]; skip = 1 }
' "$doc" > "$doc.tmp"
mv "$doc.tmp" "$doc"
