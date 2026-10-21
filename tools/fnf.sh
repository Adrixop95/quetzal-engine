#!/bin/sh
# usage: tools/fnf.sh ADDR : whole function (until ret followed by padding)
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
awk -v a="  $1:" 'index($0,a)==1{f=1} f{ if(r && ($0 ~ /nop|int3/)) exit; r=($0 ~ /\tret/); print }' $ROOT/work/stats/exe.asm | cut -c1-80 | cut -f1,3
