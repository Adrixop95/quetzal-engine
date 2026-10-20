#!/bin/sh
# usage: tools/zpath_matrix.sh OUTROOT SECONDS PARALLEL "courses" "counts" "fogs"   -> OUTROOT/<course>_<n>_<fog>/ + summary.jsonl
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; OUT=$1; SECS=$2; P=$3
mkdir -p "$OUT"
for c in $4; do for n in $5; do for f in $6; do echo "$c $n $f"; done; done; done | \
  xargs -P "$P" -L1 sh -c "$ROOT/tools/zpath_run.sh \$0 \$1 \$2 $OUT/\$0_\$1_\$2 $SECS >/dev/null 2>&1; python3 -I $ROOT/tools/zpath_analyze.py $OUT/\$0_\$1_\$2 > $OUT/\$0_\$1_\$2/summary.json 2>&1"
cat "$OUT"/*/summary.json > "$OUT/summary.jsonl"
