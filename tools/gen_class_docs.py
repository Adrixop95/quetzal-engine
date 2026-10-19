#!/usr/bin/env python3 -I
"""docs/classes.md from the recovered data.
usage: gen_class_docs.py work/class_model.json work/source_map.json work/class_edges.json work/classes.json out.md"""
import json, sys, collections
mj, sj, ej, cj, out = sys.argv[1:6]
model = json.load(open(mj)); smap = json.load(open(sj)); edges = json.load(open(ej)); cl = json.load(open(cj))
base = {k: max(v, key=v.get) for k, v in edges.items()}
kids = collections.defaultdict(list)
for k, b in base.items(): kids[b].append(k)
# object kinds from the factory (0x43d5d0): kind -> final vtable of the constructor
KIND_CTOR = {0: '00433190', 1: '0044a350', 2: '0040fb70', 3: '00471390', 4: '00460090', 5: '00407360', 6: '0042bda0', 7: '0041cce0', 8: '00408410', 9: '00415f50', 10: '0042d9b0'}
ctor2vt = collections.defaultdict(list)
for vt, c in cl.items():
    for f in c['ctors']: ctor2vt[f].append(vt)
kind_of = {}
for k, c in KIND_CTOR.items():
    for vt in ctor2vt.get(c, []):
        if len(cl[vt]['methods']) >= 70: kind_of[vt] = k

def name_of(vt):
    votes = smap['class_files'].get(vt) or []
    names = [f for f, n in votes if not f.startswith(('VisibleThing', 'ListTemplate', 'Data.'))]
    if names: return names[0].rsplit('.', 1)[0]
    if votes: return votes[0][0].rsplit('.', 1)[0]
    return None

def is_ui(vt):
    cur, seen = vt, 0
    while cur in base and seen < 30:
        cur = base[cur]; seen += 1
    return cur in ('00490000', '00490158', '004901cc', '00490c80', '00492070', '004902f4', '00490ba0', '00490c10', '00490cfc')

rows = []
for vt in sorted(cl):
    m = model.get(vt, {})
    nm = name_of(vt)
    role = ('UI widget' if is_ui(vt) else 'world object kind %d' % kind_of[vt] if vt in kind_of else '')
    rows.append((vt, nm or '', role, base.get(vt, ''), len(cl[vt]['methods']), m.get('count', 0), ','.join(sorted(m.get('sizes', {}))), len(m.get('missions', []))))
L = ['# Klasy gry (odzyskane z kodu i obserwacji)', '',
     'Wygenerowane przez `tools/gen_class_docs.py` z: tablic wirtualnych, konstruktorów, łańcuchów źródłowych (assert) i skanu żywych obiektów',
     'z 15 misji kampanii (`tools/survey.sh`). Nazwa = nazwa pliku źródłowego (`Marine.cpp` itd.) znaleziona w napisach pliku wykonywalnego; puste = brak śladu.',
     '', '| vtable | nazwa | rola | baza | metod | żywych obiektów (15 misji) | rozmiary | misji |', '|---|---|---|---|---|---|---|---|']
for r in rows: L.append('| %s | %s | %s | %s | %d | %d | %s | %d |' % r)
L += ['', '## Pliki źródłowe oryginału (z napisów assert w pliku wykonywalnym)', '', ', '.join(smap['sources']), '']
L += ['## Funkcje zmapowane na pliki źródłowe', '', '%d funkcji ma przypisany plik (po napisach assert). Pełna lista: `work/source_map.json` (generowana lokalnie).' % len(smap['func_file']), '']
open(out, 'w').write('\n'.join(L))
print('rows', len(rows), 'named', sum(1 for r in rows if r[1]), 'ui', sum(1 for r in rows if r[2] == 'UI widget'), 'objects kinds', sum(1 for r in rows if r[2].startswith('world')))
