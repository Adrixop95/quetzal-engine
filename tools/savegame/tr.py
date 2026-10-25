#!/usr/bin/env python3 -I
"""Load/print an RT_WRITE_TRACE file: tr.py TRACE LO HI"""
import sys,struct
def load(fn):
    rows=[]
    for l in open(fn):
        h,t=l.rstrip('\n').split(' | ')
        p=h.split(); off=int(p[0]); n=int(p[1]); data=bytes(int(x,16) for x in p[2:])
        rows.append((off,n,data,[int(x,16) for x in t.split()]))
    return rows
if __name__=='__main__':
    rows=load(sys.argv[1]); lo=int(sys.argv[2]); hi=int(sys.argv[3])
    for off,n,data,ch in rows:
        if lo<=off<hi:
            v=struct.unpack('<I',data[:4])[0] if n==4 else None
            print(off,n,'%s'%(('%d'%v) if v is not None else data[:8].hex()),' '.join('%x'%c for c in ch[1:5]))
