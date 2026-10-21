#!/bin/sh
# builds speed maps zspd1..N: 7 single units per map in clean rows, moving east 18 tiles; runs all in parallel
cd "$(dirname "$0")/.."
rows="1 3 4 5 6 7 8"; n=0; set -- "$@"
mk() { n=$((n+1)); name=zspd$n; args=""; set -- $rows; for t in $LIST; do args="$args 1:$t:1:1:$1:move=19,$1@1300"; shift; done; python3 -I tools/make_zstat.py $name $args; }
LIST="0 1 2 5 6 27 28" mk; LIST="29 31 32 36 53 54 55" mk; LIST="56 57 82 3 4 8 9" mk; LIST="58 59 61 62 30 35 7" mk; LIST="85 33 34 60 83 87" mk
for i in 1 2 3 4 5; do RT_UNIT_EVERY=1 tools/zstat_run.sh zspd$i work/zspd$i 30000 & done; wait
