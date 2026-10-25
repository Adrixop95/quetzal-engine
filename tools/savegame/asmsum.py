#!/usr/bin/env python3 -I
"""Disassembly slice from work/stats/exe.asm: asmsum.py START_ADDR [N]  (START must be an instruction boundary)"""
import re,sys,os
root=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..')
asm={};order=[]
for l in open(os.path.join(root,'work/stats/exe.asm')):
    m=re.match(r'\s*([0-9a-f]+):\t([0-9a-f ]+?)\s*\t(.*)',l)
    if not m: continue
    a=int(m.group(1),16); asm[a]=re.sub(r'\s+',' ',m.group(3).strip()); order.append(a)
if __name__=='__main__':
    s=int(sys.argv[1],16); n=int(sys.argv[2]) if len(sys.argv)>2 else 60
    i=order.index(s)
    for a in order[i:i+n]: print('%x %s'%(a,asm[a]))
