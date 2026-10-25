#!/bin/bash
# usage: zpath_one.sh COURSE N FOG OUTROOT SECONDS : run + analyse one course, retrying when the mission did not start properly
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; c=$1; n=$2; f=$3; OUT=$4; SECS=$5; d="$OUT/${c}_${n}_${f}"
for try in 1 2 3; do
  "$ROOT/tools/zpath_run.sh" "$c" "$n" "$f" "$d" "$SECS" >/dev/null 2>&1
  python3 -I "$ROOT/tools/zpath_analyze.py" "$d" > "$d/summary.json" 2>/dev/null
  got=$(python3 -I -c "import json;print(json.load(open('$d/summary.json'))['n'])" 2>/dev/null || echo 0)
  [ "${got:-0}" -ge $((n * 9 / 10)) ] && break
done
