"""Synthetic player-command pathing courses and offline motion measurements."""
import argparse
import heapq
import json
import math
from pathlib import Path
import struct
import sys
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import playthrough_route as route  # noqa: E402

WIDTH, HEIGHT, TILE, BUDGET = 40, 28, 48, 900
NAMES = ('narrow', 'forest', 'dense', 'allies', 'blocked', 'unreachable', 'edges', 'group', 'construction', 'water', 'shore', 'opposing')


def course(name):
    if name not in NAMES:
        raise ValueError(f'unknown course: {name}')
    terrain = [[2] * WIDTH for _ in range(HEIGHT)]
    units, goals = [(3, 12, 0)], {0: (32, 12)}
    if name in ('narrow', 'allies', 'opposing'):
        for y in range(HEIGHT):
            for x in range(18, 21):
                if y != 12:
                    terrain[y][x] = 0
    if name == 'forest':
        for y in range(HEIGHT):
            for x in range(12, 25):
                terrain[y][x] = 1 if y == 12 else 0
        units += [(16, 12, 1), (22, 12, 1)]
    if name == 'allies':
        units.append((19, 12, 1))
    if name in ('dense', 'group'):
        units = [(x, y, 0) for y in range(10, 14) for x in range(3, 7)]
        if name == 'group':
            for y in range(HEIGHT):
                if y not in (11, 12):
                    terrain[y][18] = 0
    if name == 'blocked':
        terrain[12][32] = 0
    if name == 'unreachable':
        for y in range(HEIGHT):
            terrain[y][18] = 0
    if name == 'edges':
        units, goals = [(0, 0, 0)], {0: (39, 27)}
    if name in ('water', 'shore'):
        for y in range(HEIGHT):
            for x in range(10, 30):
                terrain[y][x] = 0
        # Non-scripted type-88 units exercise the generic water mover.
        # Scripted Johnkas patrol independently; ferries use a shuttle controller.
        units, goals = [(12, 12, 88)], {88: (27, 12) if name == 'water' else (29, 12)}
    if name == 'opposing':
        units = [(x, 12, 0) for x in range(3, 7)] + [(x, 12, 1) for x in range(32, 36)]
        goals[1] = (3, 12)
    return dict(name=name, terrain=terrain, units=units, goals=goals)


def region(spec, kind):
    """Explicit accepted closest approach; never silently redefine a failed goal."""
    x, y = spec['goals'][kind]
    if spec['name'] == 'shore':
        x = 29  # Last water tile; a vessel cannot acquire a land tile.
    if spec['name'] == 'unreachable':
        x = 17  # Explicit closest approach on the start side of a solid wall.
    radius = 3 if spec['name'] in ('dense', 'group', 'opposing') else 1
    return max(0, x - radius), max(0, y - radius), min(WIDTH - 1, x + radius), min(HEIGHT - 1, y + radius)


def inside(position, area):
    x, y = (v // TILE for v in position)
    return area[0] <= x <= area[2] and area[1] <= y <= area[3]


def generate(directory, spec):
    """Invented maps and rules only; never reads an installation or research data."""
    game = route.demo_game(directory)
    base = game / 'Maps/demo'
    units = spec['units']
    town = spec['name'] == 'construction'
    text = (f'[Globals]\nMapSize={WIDTH},{HEIGHT}\nSides=1\nTeams={int(bool(units))}\n'
            f'Towns={int(town)}\nJohnkas=0\n[Side0]\nStart_X=3\nStart_Y=12\nStartMoney=10000\n')
    if town:
        text += '[Town0]\nSide=0\nX=16\nY=7\nType=0\nRace=0\n'
    if units:
        text += f'[Team0]\nSide=0\nUnits={len(units)}\n'
        text += ''.join(f'Unit{i}={x},{y},{kind},100\n' for i, (x, y, kind) in enumerate(units))
    base.with_suffix('.mp').write_text(text)
    base.with_suffix('.map').write_bytes(struct.pack('<HH', WIDTH, HEIGHT) + bytes(WIDTH * HEIGHT * 4))
    base.with_suffix('.dat').write_bytes(bytes(v for row in spec['terrain'] for v in row))
    base.with_suffix('.rm').write_bytes(struct.pack('<HH', WIDTH, HEIGHT) + bytes([255, 0]) * WIDTH * HEIGHT)
    conditions = ['localTime >= 850']
    for kind in spec['goals']:
        area = ','.join(map(str, region(spec, kind)))
        count = sum(u[2] == kind for u in spec['units'])
        # Regions do not contain the idle ally or the other column's goal.
        conditions.append(f'side(local).unitsInRect({area}) == {count}')
    base.with_suffix('.scn').write_text('trigger(0) conditions ' + ' '.join(conditions) + ' actions side(local).victory end\n')
    rules = directory / 'rules'
    rules.mkdir()
    (rules / 'types.csv').write_text('id,ugi,kind,param,price,time,level\n0,SYNTHETIC,Marine,0,10,100,0\n1,ALLY,Marine,1,10,100,0\n14,STRUCTURE,Building,0,100,1200,0\n')
    # Two invented descriptors: fixed speed 8, small movement class, no weapons.
    descriptor = [0] * 20
    descriptor[2], descriptor[6], descriptor[8], descriptor[12] = 1000, 8, -1, 1
    rows = [','.join(map(str, [i] + descriptor[1:])) for i in range(2)]
    (rules / 'desc_marine.csv').write_text('param,' + ','.join(f'd{i}' for i in range(19)) + '\n' + '\n'.join(rows) + '\n')
    (rules / 'townhall_ids.csv').write_text('level,race0,race1,race2\n0,10,37,62\n')
    (rules / 'buildtree.csv').write_text('race,hall,slot,type,construct,prereq\n0,0,0,14,1,\n')
    script = ''
    if town:
        script += 'select type 10\nbuild 14 19 12 2\nwait 2\n'
    for kind, (x, y) in spec['goals'].items():
        script += f'select type {kind}\n'
        if spec['name'] == 'group':
            script += 'group assign 1\nselect group 1\n'
        script += f'order-move {x} {y}\n'
    script += f'await {BUDGET - 10} outcome == 1\ncheckpoint outcome == 1\nquit\n'
    return game, rules, script


def grid_distance(spec, start, area, water=False):
    """Exact shortest 8-neighbour centre route to the accepted region, no crowd."""
    terrain = spec['terrain']

    def free(x, y):
        construction = spec['name'] == 'construction' and ((19 <= x <= 20 and 12 <= y <= 13)
                                                         or (16 <= x <= 17 and 7 <= y <= 8))
        return (0 <= x < WIDTH and 0 <= y < HEIGHT and not construction
                and (terrain[y][x] == 0 if water else terrain[y][x] >= 1))

    queue, distance = [(0.0, *start)], {start: 0.0}
    while queue:
        cost, x, y = heapq.heappop(queue)
        if cost != distance[x, y]:
            continue
        if inside((x * TILE + 24, y * TILE + 24), area):
            return cost
        for dx, dy in ((0, 1), (0, -1), (1, 0), (-1, 0), (1, 1), (-1, 1), (1, -1), (-1, -1)):
            nx, ny = x + dx, y + dy
            if not free(nx, ny) or (dx and dy and (not free(nx, y) or not free(x, ny))):
                continue
            candidate = cost + TILE * math.hypot(dx, dy)
            if candidate < distance.get((nx, ny), math.inf):
                distance[nx, ny] = candidate
                heapq.heappush(queue, (candidate, nx, ny))
    return None


def measure(rows, spec, *, stall=120):
    """Measure command episodes, including motion that oscillates without progress."""
    episodes, previous = {}, {}
    for row in rows:
        objects = {o['id']: o for o in row['objects']}
        for cmd in row['commands']:
            if cmd['type'] != 0x17:
                continue
            for oid in cmd['ids']:
                obj = objects[oid]
                initial = previous.get(oid, obj)['position']
                area = region(spec, obj['type'])
                baseline = grid_distance(spec, tuple(v // TILE for v in initial), area, obj['type'] == 88)
                episodes[oid] = dict(object=oid, start=row['tick'], arrival_tick=None, settlement_tick=None, path_pixels=0.0,
                                     optimal_grid_ticks=None if baseline is None else baseline / 8,
                                     time_ratio=None, jitter=0, oscillation=0, stuck=0, max_stall=0,
                                     jumps=0, path_failures=0, area=area, last=initial, velocity=None, history=[],
                                     best=math.inf, progress=row['tick'], stationary=row['tick'])
        for oid, e in episodes.items():
            if oid not in objects:
                continue
            obj = objects[oid]
            e['path_failures'] = obj['path_failures']
            pos = obj['position']
            delta = tuple(a - b for a, b in zip(pos, e['last']))
            length = math.hypot(*delta)
            e['path_pixels'] += length
            e['jumps'] += length > 12  # Synthetic speed is 8; flag recovery relocation separately.
            if length > 0:
                e['stationary'] = row['tick']
                old = e['velocity']
                if old and sum(a * b for a, b in zip(old, delta)) < 0:
                    e['jitter'] += 1
                e['velocity'] = delta
            tile = tuple(v // TILE for v in pos)
            if not e['history'] or e['history'][-1] != tile:
                if len(e['history']) >= 2 and e['history'][-2] == tile:
                    e['oscillation'] += 1
                e['history'].append(tile)
            area = e['area']
            # Distance to accepted rectangle; a looping mover cannot reset this watchdog.
            dx = max(area[0] * TILE - pos[0], 0, pos[0] - ((area[2] + 1) * TILE - 1))
            dy = max(area[1] * TILE - pos[1], 0, pos[1] - ((area[3] + 1) * TILE - 1))
            distance = math.hypot(dx, dy)
            if distance < e['best'] - 1:
                e['best'], e['progress'] = distance, row['tick']
            e['max_stall'] = max(e['max_stall'], row['tick'] - e['stationary'])
            if obj['order'] and row['tick'] - e['stationary'] == stall:
                e['stuck'] += 1
            if inside(pos, area) and obj['alive'] and not obj['carrier']:
                if e['arrival_tick'] is None:
                    e['arrival_tick'] = row['tick'] - e['start'] + 1
                    ideal = e['optimal_grid_ticks']
                    e['time_ratio'] = e['arrival_tick'] / ideal if ideal else None
                if not obj['order'] and e['settlement_tick'] is None:
                    e['settlement_tick'] = row['tick'] - e['start'] + 1
            e['last'] = pos
        previous = objects
    final_tick = rows[-1]['tick'] if rows else 0
    values = []
    for e in episodes.values():
        obj = previous.get(e['object'])
        e['settled'] = bool(obj and obj['alive'] and not obj['carrier'] and not obj['order'] and inside(obj['position'], e['area']))
        e['deadlock'] = not e['settled'] and final_tick - e['progress'] >= stall
        for key in ('area', 'last', 'velocity', 'history', 'best', 'progress', 'stationary'):
            del e[key]
        values.append(e)
    ratios = [e['time_ratio'] for e in values if e['time_ratio'] is not None]
    arrivals = [e['arrival_tick'] for e in values if e['arrival_tick'] is not None]
    return dict(ordered=len(values), arrived=sum(e['arrival_tick'] is not None for e in values),
                arrival_rate=sum(e['arrival_tick'] is not None for e in values) / len(values) if values else 0,
                stuck=sum(e['stuck'] for e in values), deadlocks=sum(e['deadlock'] for e in values),
                settled=sum(e['settled'] for e in values), last_arrival=max(arrivals, default=None),
                max_time_ratio=max(ratios, default=None), jitter=sum(e['jitter'] for e in values),
                oscillation=sum(e['oscillation'] for e in values), jumps=sum(e['jumps'] for e in values), units=values)


def run(directory, engine, spec, profile, *, launcher=('unshare', '-rn')):
    directory.mkdir()
    game, rules, script = generate(directory / 'game', spec)
    args = SimpleNamespace(engine=engine, rules_dir=rules, capture_every=None, max_ticks=BUDGET, mode='headless',
                           timeout=30, goal_ticks=BUDGET + 1, stuck_ticks=120, idle_ticks=BUDGET + 1,
                           economy_ticks=BUDGET + 1, path_failures=1000, profile=profile)
    report = route.execute(args, directory / 'route', game, 'demo', script, launcher=launcher)
    try:
        rows = list(route.read_observations(directory / 'route/observations.jsonl'))
    except (OSError, ValueError):
        rows = []  # The framework report retains namespace/process/observation failures.
    metrics = measure(rows, spec)
    result = dict(course=spec['name'], profile=profile, route=report, metrics=metrics)
    (directory / 'metrics.json').write_text(json.dumps(result, indent=2) + '\n')
    return result, rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/aztec_engine')
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--courses', nargs='+', choices=NAMES, default=NAMES)
    parser.add_argument('--profiles', nargs='+', choices=('faithful', 'enhanced'), default=('faithful', 'enhanced'))
    args = parser.parse_args()
    output = args.out.resolve()
    if not output.is_relative_to(ROOT / 'work') or output.exists():
        parser.error('--out must be a new directory inside this worktree work/')
    output.mkdir(parents=True)
    results = []
    for name in args.courses:
        for profile in args.profiles:
            result, _ = run(output / f'{name}-{profile}', args.engine.resolve(), course(name), profile)
            results.append(result)
    (output / 'report.json').write_text(json.dumps(results, indent=2) + '\n')
    lines = ['| Course | Profile | Arrival | Settled | Last arrival | Max grid time ratio | Stuck | Jitter | Oscillation | Deadlocks | Jumps |',
             '| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |']
    for r in results:
        m = r['metrics']
        lines.append(f'| {r["course"]} | {r["profile"]} | {m["arrived"]}/{m["ordered"]} | {m["settled"]} | '
                     f'{m["last_arrival"]} | {m["max_time_ratio"]} | {m["stuck"]} | {m["jitter"]} | '
                     f'{m["oscillation"]} | {m["deadlocks"]} | {m["jumps"]} |')
    (output / 'report.md').write_text('\n'.join(lines) + '\n')
    return int(any(r['route']['outcome'] != 'won' or r['metrics']['arrival_rate'] != 1
                   or r['metrics']['settled'] != r['metrics']['ordered'] or r['metrics']['stuck']
                   or r['metrics']['deadlocks'] for r in results))


if __name__ == '__main__':
    raise SystemExit(main())
