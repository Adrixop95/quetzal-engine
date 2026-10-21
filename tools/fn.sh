#!/bin/sh
# usage: tools/fn.sh ADDR [maxlines]: disassembly from ADDR (needs work/stats/exe.asm: objdump -D -b binary -mi386 -M intel --adjust-vma=0x400000 --start-address=0x401000 --stop-address=0x48e382 game/Aztec.exe)
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
awk -v a="  $1:" -v m="${2:-60}" 'index($0,a)==1{f=1} f{print; n++; if(n>=m) exit}' $ROOT/work/stats/exe.asm | cut -c1-80
