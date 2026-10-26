#!/bin/bash
# usage: tools/zpath_matrix.sh OUTROOT SECONDS PARALLEL "courses" "counts" "fogs"   -> OUTROOT/<course>_<n>_<fog>/summary.json + summary.jsonl
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT=$1; SECS=$2; P=$3
mkdir -p "$OUT"
for c in $4; do for n in $5; do for f in $6; do echo "$c $n $f $OUT $SECS"; done; done; done | xargs -P "$P" -L1 "$ROOT/tools/zpath_one.sh"
cat "$OUT"/*/summary.json > "$OUT/summary.jsonl"
