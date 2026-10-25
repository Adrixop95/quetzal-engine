#!/usr/bin/env python3 -I
"""Print decompiled functions from work/decomp/all.c: fn.py ADDR... (8 hex digits)"""
import re,sys,os
root=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..')
t=open(os.path.join(root,'work/decomp/all.c')).read()
for a in sys.argv[1:]:
    a=a.zfill(8)
    m=re.search(r'// ==== %s [^\n]*\n(.*?)(?=\n// ==== |\Z)'%a,t,re.S)
    print(m.group(0) if m else 'none '+a)
