#!/usr/bin/env python3 -I
"""Symbolic summary of a save/load serializer function: ser.py ADDR  -> W32/WBUF/CALL events with value sources."""
import re,sys,bisect,os
root=os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..')
asm={};order=[]
for l in open(os.path.join(root,'work/stats/exe.asm')):
    m=re.match(r'\s*([0-9a-f]+):\t([0-9a-f ]+?)\s*\t(.*)',l)
    if not m: continue
    a=int(m.group(1),16); asm[a]=re.sub(r'\s+',' ',m.group(3).strip()); order.append(a)
fs=sorted(int(l.split()[0],16) for l in open(os.path.join(root,'work/decomp/functions.txt')))
HELP={0x444ba0:'IDX_UNIT',0x444bd0:'IDX_OBJ',0x444c30:'IDX_C30',0x444c60:'IDX_C60',0x444cb0:'IDX_CB0',0x444ce0:'IDX_CE0',0x444d10:'IDX_D10',0x444d40:'IDX_D40'}
def run(start,end=None):
    if end is None:
        k=bisect.bisect_right(fs,start); end=fs[k] if k<len(fs) else start+0x2000
    i=order.index(start); regs={}; stack=[]; targets=set(); lines=[]
    for a in order[i:]:
        if a>=end: break
        m=re.match(r'j\w+ 0x([0-9a-f]+)$',asm[a])
        if m: targets.add(int(m.group(1),16))
    for a in order[i:]:
        if a>=end: break
        ins=asm[a]
        if a in targets: lines.append('%x: L%x:'%(a,a))
        m=re.match(r'(\w+) (.*)',ins)
        op=m.group(1) if m else ins; arg=m.group(2) if m else ''
        val=lambda x: regs.get(x.strip(),x.strip())
        if op in('mov','movsx','movzx'):
            d,s=[t.strip() for t in arg.split(',',1)]
            if re.match(r'(e?[abcd]x|e?[sd]i|[abcd]l|ebp)$',d):
                regs[d]=val(s)
                if d in('al','cl','dl','bl'): regs['e'+d[0]+'x']=val(s)
            continue
        if op=='xor':
            d,s=[t.strip() for t in arg.split(',',1)]
            if d==s: regs[d]='0'
            continue
        if op=='lea':
            d,s=[t.strip() for t in arg.split(',',1)]; regs[d]='&'+s; continue
        if op=='push': stack.append(val(arg)); continue
        if op=='pop': regs[arg.strip()]='?'; continue
        if op=='call':
            t=arg.strip(); tv=int(t[2:],16) if t.startswith('0x') else None
            if tv==0x444f80:
                v=stack.pop() if stack else '?'
                if v=='?' or v.startswith('ret('):
                    k=order.index(a); ctx=' ; '.join(asm[order[q]] for q in range(max(0,k-7),k))
                    v+='   <<'+ctx+'>>'
                mm=re.search(r'\[(e[a-z]{2})(\+0x[0-9a-f]+)?\]',v)
                if mm and regs.get(mm.group(1)) and mm.group(1)!='esp': v+='   {%s=%s}'%(mm.group(1),regs[mm.group(1)])
                lines.append('%x: W32 %s'%(a,v))
            elif tv==0x444fb0:
                p=stack.pop() if stack else '?'; n=stack.pop() if stack else '?'
                lines.append('%x: WBUF ptr=%s len=%s'%(a,p,n))
            elif tv in HELP:
                v=stack.pop() if stack else '?'; regs['eax']='%s(%s)'%(HELP[tv],v)
            else:
                lines.append('%x: CALL %s args=%s ecx=%s'%(a,t,stack[-3:],regs.get('ecx','?')))
                regs['eax']='ret(%s)'%t; regs['ecx']='?'; regs['edx']='?'
            continue
        if op=='add' and arg.startswith('esp,'):
            for _ in range(int(arg.split(',')[1],16)//4):
                if stack: stack.pop()
            continue
        if op.startswith('j'): lines.append('%x: %s'%(a,ins)); continue
        if op in('inc','dec','cmp','test','sub','and','or','shl','shr','sar','imul','neg','not','add'):
            if op in('cmp','test'): lines.append('%x: %s'%(a,ins))
            else:
                d=arg.split(',')[0].strip()
                if d in regs: regs[d]='?'
            continue
        if op.startswith('ret'): lines.append('%x: RET'%a)
    return lines
if __name__=='__main__':
    print('\n'.join(run(int(sys.argv[1],16),int(sys.argv[2],16) if len(sys.argv)>2 else None)))
