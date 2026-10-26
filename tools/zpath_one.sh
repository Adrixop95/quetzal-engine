#!/bin/sh
# usage: zpath_one.sh COURSE N FOG OUTROOT SECONDS
# Failures remain failures; every invocation needs a fresh output directory.
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
c=$1; n=$2; f=$3; OUT=$4; SECS=$5; d="$OUT/${c}_${n}_${f}"
"$ROOT/tools/zpath_run.sh" "$c" "$n" "$f" "$d" "$SECS"
python3 -I "$ROOT/tools/zpath_analyze.py" "$d" > "$d/summary.json"
