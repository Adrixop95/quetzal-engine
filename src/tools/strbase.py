#!/usr/bin/env python3 -I
"""Parser for Res/strbase.bds (text string database): strbase.py game/Res/strbase.bds [out.json]
Format: *STRING { *ID 0xHEX *ORDINAL n *LANGUAGE { *ENGLISH "text" *RUSSIAN "text(cp1250)" } } ..."""
import re, sys, json
d = open(sys.argv[1], 'rb').read().decode('cp1250')
res = []
for blk in re.split(r'\*STRING\b', d)[1:]:
    m_id = re.search(r'\*ID\s+(0x[0-9A-Fa-f]+)', blk)
    m_or = re.search(r'\*ORDINAL\s+(\d+)', blk)
    e = re.search(r'\*ENGLISH\s+"((?:[^"\\]|\\.)*)"', blk)
    r = re.search(r'\*RUSSIAN\s+"((?:[^"\\]|\\.)*)"', blk)
    res.append({'id': int(m_id.group(1), 16) if m_id else None, 'ordinal': int(m_or.group(1)) if m_or else None,
                'en': e.group(1) if e else '', 'pl': r.group(1) if r else ''})
if len(sys.argv) > 2: json.dump(res, open(sys.argv[2], 'w'), ensure_ascii=False, indent=0)
print(len(res), 'strings')
for r in res[:5]: print(r)
