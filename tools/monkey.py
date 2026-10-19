#!/usr/bin/env python3 -I
"""Generate a random-input scenario for an in-mission session.
usage: monkey.py MISSION(1-9) SEED SECONDS > scenario.txt   (800x600 coordinates)"""
import random, sys
m, seed, secs = int(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
r = random.Random(seed)
y = 257 + 20 * (m - 1)
out = open(__file__.rsplit('/', 2)[0] + '/tests/scenarios/mission1_start.txt').read().replace('click 150 257', f'click 150 {y}')
out = out.replace('wait 6000\n', 'wait 6000\n', 1)
out += 'wait 2000\n'
t = 0
def pt(): return r.randint(5, 535), r.randint(25, 590)
while t < secs * 1000:
    k = r.random()
    if k < 0.30:
        x, yy = pt(); out += f'click {x} {yy}\n'
    elif k < 0.55:
        x, yy = pt(); out += f'rclick {x} {yy}\n'
    elif k < 0.65:
        x, yy = pt(); x2, y2 = pt(); out += f'move {x} {yy}\ndown\nwait 100\ndragto {x2-x} {y2-yy}\nwait 100\nup\n'
    elif k < 0.85:   # right panel: build/unit icons and minimap
        x, yy = r.randint(605, 795), r.randint(60, 590); out += f'click {x} {yy}\n'
    elif k < 0.93:
        out += f'key {r.choice([2,3,4,5,6,7,8,9,30,31,32,33,35,38,46,47,48,49,50,57,200,208,203,205])}\n'
    else:
        x, yy = pt(); out += f'move {x} {yy}\n'
    d = r.choice([150, 300, 600, 1200]); out += f'wait {d}\n'; t += d + 200
out += 'wait 1000\nshot\nquit\n'
print(out)
