#!/usr/bin/env python3 -I
"""Pathing test courses. usage: make_zpath.py COURSE NUNITS OUTDIR [fog|explored]
Writes OUTDIR/Maps/zpath1.{map,dat,rm,vis,spr,scn,mit,mp} (copy of mission_1 assets with replaced passability/units),
OUTDIR/cmd.txt (RT_CMD_REPLAY file: box-select + move orders) and OUTDIR/course.json (grid, goal, units, orders) for analysis.
All coordinates are tiles (48 px). Units are footmen (UGI 0) of side 0, created in [Town0]."""
import sys, os, json, shutil, struct, re
W, H = 90, 60
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'game', 'Maps') + '/'

def grid():
    g = [[2] * W for _ in range(H)]
    for x in range(W):
        for y in (0, 1, H - 2, H - 1): g[y][x] = 0
    for y in range(H):
        for x in (0, 1, W - 2, W - 1): g[y][x] = 0
    return g
def rect(g, x0, y0, x1, y1, v=0):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1): g[y][x] = v

# type id -> (speed px/tick (Marine descriptor dword 5), small?, hp); hp values from docs/type_stats.md
TYPES = {0: (9, 1, 6000), 1: (6, 1, 5000), 2: (7, 1, 8500), 5: (5, 1, 36000), 6: (15, 0, 10000), 27: (8, 1, 3500), 28: (5, 1, 6000), 29: (10, 1, 2600),
         31: (16, 1, 23000), 32: (12, 0, 23000), 53: (11, 1, 7500), 54: (10, 1, 6000), 55: (10, 1, 5000), 56: (4, 0, 22500), 57: (4, 0, 28000), 82: (10, 1, 3000)}

def course(name):
    g = grid(); goal = (80, 30); start = (6, 30); orders = None
    if name == 'open': pass
    elif name.startswith('gap'):          # vertical wall with a gap of given width
        w = int(name[3:] or 1); rect(g, 44, 2, 46, H - 3); rect(g, 44, 30 - w // 2, 46, 30 - w // 2 + w - 1, 2)
    elif name.startswith('corr'):         # corridor of width w and length 20
        w = int(name[4:] or 1); rect(g, 40, 2, 60, H - 3); rect(g, 40, 30 - w // 2, 60, 30 - w // 2 + w - 1, 2)
    elif name == 'maze':                  # serpentine: 5 walls, gaps alternate top/bottom
        for i, x in enumerate(range(20, 70, 10)):
            rect(g, x, 2, x + 1, H - 3)
            if i % 2 == 0: rect(g, x, H - 8, x + 1, H - 3, 2)
            else: rect(g, x, 2, x + 1, 8, 2)
    elif name == 'utrap':                 # concave U facing the army, goal inside/behind it
        rect(g, 50, 14, 52, 46); rect(g, 40, 14, 52, 16); rect(g, 40, 44, 52, 46); goal = (62, 30)
        rect(g, 50, 29, 52, 31, 0)
    elif name == 'wall':                  # long wall, detour around both ends is far
        rect(g, 44, 6, 46, H - 7)
    elif name == 'rooms':                 # two rooms joined by 2 doors, goal far side
        rect(g, 44, 2, 46, H - 3); rect(g, 44, 14, 46, 15, 2); rect(g, 44, 44, 46, 45, 2)
        rect(g, 20, 20, 30, 40)   # pillar room block in start side
    elif name.startswith('swap'):         # two groups swap sides through a gap (default 3 wide)
        w = int(name[4:] or 3); rect(g, 44, 2, 46, H - 3); rect(g, 44, 30 - w // 2, 46, 30 - w // 2 + w - 1, 2)
        orders = 'swap'
    elif name == 'park':                  # 2-wide corridor; group A parks just outside its exit, then group B must pass through them
        rect(g, 40, 2, 60, H - 3); rect(g, 40, 29, 60, 30, 2); orders = 'park'; goal = (80, 30)
    elif name == 'mixed':                 # open field, units of different speed/size classes ordered together
        orders = 'mixed'
    elif name == 'diag':                  # diagonal wall (corner cutting)
        for i in range(0, 44): rect(g, 20 + i, 8 + i, 21 + i, 9 + i)
        rect(g, 62, 50, 64, 52, 2)
    else: raise SystemExit('unknown course ' + name)
    return g, start, goal, orders

def bfs_len(g, a, b):
    """optimal 8-neighbour path length in px (octile, no corner cutting through blocked diagonals)"""
    import heapq
    D = [(1, 0, 48), (-1, 0, 48), (0, 1, 48), (0, -1, 48), (1, 1, 68), (1, -1, 68), (-1, 1, 68), (-1, -1, 68)]
    dist = {a: 0}; pq = [(0, a)]
    while pq:
        d, p = heapq.heappop(pq)
        if p == b: return d
        if d > dist[p]: continue
        for dx, dy, c in D:
            q = (p[0] + dx, p[1] + dy)
            if not (0 <= q[0] < W and 0 <= q[1] < H) or g[q[1]][q[0]] == 0: continue
            if dx and dy and (g[p[1]][q[0]] == 0 or g[q[1]][p[0]] == 0): continue
            nd = d + c
            if nd < dist.get(q, 1 << 30): dist[q] = nd; heapq.heappush(pq, (nd, q))
    return None

def main():
    name, n, out = sys.argv[1], int(sys.argv[2]), sys.argv[3]
    fog = (sys.argv[4] if len(sys.argv) > 4 else 'fog')
    g, start, goal, orders = course(name)
    os.makedirs(out + '/Maps', exist_ok=True)
    for e in ('map', 'spr', 'mit', 'vis'):
        shutil.copy(SRC + 'mission_1.' + e, out + '/Maps/zpath1.' + e)
    open(out + '/Maps/zpath1.dat', 'wb').write(bytes(v for row in g for v in row))
    open(out + '/Maps/zpath1.rm', 'wb').write(struct.pack('<HH', W, H) + bytes(2 * W * H))
    if fog == 'explored':
        vis = struct.pack('<HHH', 2, W, H) + struct.pack('<%dH' % (W * H), *([0xffff] * (W * H)))
        open(out + '/Maps/zpath1.vis', 'wb').write(vis)
    open(out + '/Maps/zpath1.scn', 'w', newline='').write('trigger(0)\r\n  conditions\r\n    localTime == 99999999\r\n  actions\r\n    side(1).defeat\r\nend\r\n')
    t = open(SRC + 'mission_1.mp', encoding='latin1').read()
    head = t.split('[Town0]')[0]
    head = re.sub(r'MapName=.*', 'MapName=zpath1', head); head = re.sub(r'Name=Mission 1', 'Name=ZPath1', head)
    head = re.sub(r'Teams=\d+', 'Teams=1', head); head = re.sub(r'CurrentTeam=\d+', 'CurrentTeam=0', head)
    head = re.sub(r'Start_X=\d+', 'Start_X=%d' % start[0], head, count=1); head = re.sub(r'Start_Y=\d+', 'Start_Y=%d' % start[1], head, count=1)
    types = [int(x) for x in os.environ.get('ZP_TYPES', '0' if orders != 'mixed' else '0,6,57,31,32').split(',')]
    units = []   # (x, y, group, type)
    groups = [dict(goal=goal, frame=60)]
    if orders == 'swap': groups.append(dict(goal=start, frame=64))
    if orders == 'park':
        na = max(1, n // 3); groups = [dict(goal=(64, 30), frame=60), dict(goal=goal, frame=260)]
    for i in range(n):
        if orders == 'swap':
            grp = 1 if i >= n // 2 else 0; k = i - (n // 2 if grp else 0)
            x = start[0] + (66 if grp else 0) + k % 5; y = start[1] - 4 + k // 5
        elif orders == 'park':
            grp = 0 if i < na else 1; k = i if grp == 0 else i - na
            if grp == 0: x = 30 + k % 5; y = 28 + k // 5
            else: x = start[0] + k % 5; y = start[1] - 4 + k // 5
        else:
            grp = 0; x = start[0] + i % 5; y = start[1] - 4 + i // 5
        units.append((x, y, grp, types[i % len(types)]))
    # side-0 units live in a [Team] (not a [Town]) so they have no "return to town" behaviour; side 0 still needs a town object
    head = re.sub(r'Teams=\d+', 'Teams=2', head)
    body = '[Town0]\nScenarioID=0\nSide=0\nX=4\nY=4\nType=2\nTendency=0\nBuildSpeed=1\nRace=0\nUnit0= 4, 8,  0, 6000\nUnits=1\n\n'
    body += '[Team0]\nScenarioID=1\nSide=0\n'
    for i, (x, y, _, ty) in enumerate(units): body += 'Unit%d=%3d,%3d,%3d,%5d\n' % (i, x, y, ty, TYPES.get(ty, (0, 0, 6000))[2])
    body += 'Units=%d\n\n[Team1]\nScenarioID=2\nSide=1\nUnit0= 85, 56, 27, 3500\nUnits=1\n' % len(units)
    open(out + '/Maps/zpath1.mp', 'w', encoding='latin1', newline='').write(head + body)
    # commands: box select of the start block, then a move to the goal (pixel centre of goal tile)
    lines = []
    def cmd(frame, typ, d5, d6, d7=0, d8=0, d9=0):
        d = [0] * 53; d[2] = typ; d[3] = frame; d[5], d[6], d[7], d[8], d[9] = d5, d6, d7, d8, d9
        lines.append('%d 0 ' % frame + ' '.join('%x' % (v & 0xffffffff) for v in d))
    def box(us):
        xs = [u[0] for u in us]; ys = [u[1] for u in us]
        return (min(xs) * 48 - 200, min(ys) * 48 - 250, max(xs) * 48 + 250, max(ys) * 48 + 250)
    for gi, gd in enumerate(groups):
        us = [u for u in units if u[2] == gi]
        if not us: continue
        cmd(gd['frame'], 0x15, *box(us)); cmd(gd['frame'] + 2, 0x18, gd['goal'][0] * 48 + 24, gd['goal'][1] * 48 + 24)
    goals = {gi: gd['goal'] for gi, gd in enumerate(groups)}
    open(out + '/cmd.txt', 'w').write('\n'.join(lines) + '\n')
    def gstart(gi):
        us = [u for u in units if u[2] == gi]
        return (us[0][0], us[0][1]) if us else start
    opt = {gk: bfs_len(g, gstart(gk), gv) for gk, gv in goals.items()}
    json.dump({'name': name, 'n': n, 'W': W, 'H': H, 'grid': [''.join(map(str, r)) for r in g], 'start': start, 'goal': goal,
               'units': units, 'goals': {str(k): v for k, v in goals.items()}, 'order_frames': {str(gi): gd['frame'] + 2 for gi, gd in enumerate(groups)}, 'order_frame': groups[0]['frame'] + 2, 'types': types, 'fog': fog, 'optimal_px': opt},
              open(out + '/course.json', 'w'))
main()
