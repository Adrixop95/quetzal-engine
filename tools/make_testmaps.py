#!/usr/bin/env python3
"""Generate private campaign-derived showcase and combat missions; never read an EXE."""
import argparse
import configparser
import csv
import json
from pathlib import Path
import re
import shutil
import struct

ROOT = Path(__file__).resolve().parents[1]
FRACTIONS = (100, 66, 33, 10)
STRUCTURES = {'Building', 'Defence'}
HP_FIELDS = {'Marine': 1, 'Headtruck': 3, 'Spider': 6, 'Zeppelin': 1,
             'Bird': 1, 'Truck': 4, 'Cannon': 5, 'Building': 1, 'Defence': 3}
RACES = ('Rus', 'Aztec', 'Chinese')
ORDERS = ('idle', 'move', 'path', 'inverted-path', 'cycle', 'inverted-cycle', 'locked')
EXTENSIONS = ('map', 'dat', 'rm', 'vis', 'spr')


def read_ini(path):
    ini = configparser.ConfigParser(interpolation=None, strict=True)
    ini.optionxform = str
    ini.read_string(path.read_text('cp1250'))
    return ini


def find(directory, name):
    matches = sorted(p for p in directory.iterdir() if p.name.lower() == name.lower())
    if len(matches) != 1:
        raise ValueError(f'Expected exactly one {name} in {directory}')
    return matches[0]


def metadata():
    with (ROOT / 'docs/data/types.csv').open() as stream:
        rows = list(csv.DictReader(stream))
    for row in rows:
        row['id'] = int(row['id'])
        kind = row['kind']
        if kind in HP_FIELDS:
            with (ROOT / f'docs/data/desc_{kind.lower()}.csv').open() as stream:
                desc = next(d for d in csv.DictReader(stream) if d['param'] == row['param'])
            row['hp'] = int(desc[f'd{HP_FIELDS[kind]}'])
        row['race'] = 0 if row['id'] < 27 else 1 if row['id'] < 53 else 2 if row['id'] < 82 else -1
    return rows


class Mission:
    def __init__(self, name, w, h, dat, tileset):
        self.name, self.w, self.h, self.dat = name, w, h, dat
        self.sections = {}
        self.groups = {k: [] for k in ('Side', 'Town', 'Team', 'Boat', 'Johnka')}
        self.occupied = []
        self.free = {}
        self.row_bounds = (0, h)
        self.placements = []
        self.triggers = []
        self.labels = []
        self.globals = dict(Version=1, MapName=name, Name=name, MapSize=f'{w},{h}',
                            TileSet=tileset, StartTime=1800, Winds=0, MaxPlayers=1,
                            Music=0, MusicMask=0, AVI='', CurrentTeam=0, Submarines=0)

    def section(self, kind, **values):
        index = len(self.groups[kind])
        self.groups[kind].append(values)
        self.sections[f'{kind}{index}'] = values
        return index

    def slot(self, x, y, radius=1):
        """Nearest clear footprint, stable ties; reserve room around every object."""
        if radius not in self.free:
            self.free[radius] = {(xx, yy)
                                 for yy in range(radius + 2, self.h - radius - 2)
                                 for xx in range(radius + 2, self.w - radius - 2)
                                 if all(self.dat[b * self.w + a] == 2
                                        for b in range(yy - radius, yy + radius + 1)
                                        for a in range(xx - radius, xx + radius + 1))
                                 and all(max(abs(xx - a), abs(yy - b)) >= radius + r + 2
                                         for a, b, r in self.occupied)}
        chosen = min((p for p in self.free[radius]
                      if self.row_bounds[0] + radius <= p[1] < self.row_bounds[1] - radius),
                     key=lambda p: ((p[0] - x) ** 2 + (p[1] - y) ** 2, p[1], p[0]), default=None)
        if chosen is not None:
            xx, yy = chosen
            self.occupied.append((xx, yy, radius))
            for r, points in self.free.items():
                gap = radius + r + 1
                points.difference_update((a, b) for b in range(yy - gap, yy + gap + 1)
                                         for a in range(xx - gap, xx + gap + 1))
            return xx, yy
        raise ValueError(f'{self.name}: insufficient clear terrain; choose a larger/open campaign template')

    def side(self, race, computer=False, neutral=False):
        return self.section('Side', Name=RACES[race], Race=race, Color=len(self.groups['Side']),
                            Neutral=int(neutral), OnlyComp=int(computer), Difficulty=100,
                            StartMoney=1000000, DisabledID='', Start_X=12, Start_Y=12,
                            CamerasCount=1, Camera0='12,12,0', DefenceCount=0)

    def town(self, side, race, level, x, y):
        x, y = self.slot(x, y, 2)
        idx = len(self.groups['Town'])
        self.section('Town', Side=side, Race=race, Type=level, X=x, Y=y,
                     ScenarioID=100 + idx, Units=0, Tendency=0, BuildSpeed=1)
        return self.groups['Town'][-1]

    def unit(self, group, row, x, y, fraction=100):
        target = [x, y]
        x, y = self.slot(x, y, 2 if row['kind'] in STRUCTURES else 1)
        n = group['Units']
        hp = max(1, row['hp'] * fraction // 100)
        group[f'Unit{n}'] = f'{x},{y},{row["id"]},{hp}'
        group['Units'] += 1
        self.placements.append(dict(type=row['id'], side=group['Side'], x=x, y=y,
                                    hp=hp, fraction=fraction, target=target))

    def team(self, side):
        idx = len(self.groups['Team'])
        self.section('Team', Side=side, ScenarioID=idx + 1, Units=0)
        return self.groups['Team'][-1]

    def trigger(self, condition, *actions):
        idx = len(self.triggers)
        self.triggers.append(f'trigger({idx})\n  conditions\n    {condition}\n  actions\n'
                             + ''.join(f'    {a}\n' for a in actions) + 'end\n')

    def write(self, maps):
        for kind, count in (('Side', 'Sides'), ('Town', 'Towns'), ('Team', 'Teams'),
                            ('Boat', 'Boats'), ('Johnka', 'Johnkas')):
            self.globals[count] = len(self.groups[kind])
        self.globals['Notes'] = len(self.labels)
        for i, label in enumerate(self.labels):
            self.globals[f'Note{i}'] = label
        sections = {'Globals': self.globals, **self.sections}
        text = '\n'.join(f'[{name}]\n' + ''.join(f'{k}={v}\n' for k, v in values.items())
                         for name, values in sections.items())
        (maps / f'{self.name}.mp').write_bytes(text.replace('\n', '\r\n').encode('ascii'))
        (maps / f'{self.name}.scn').write_bytes(('\n'.join(self.triggers) + '\n').replace('\n', '\r\n').encode('ascii'))
        (maps / f'{self.name}.layout.json').write_text(json.dumps(dict(
            map_kind='campaign-derived', labels=self.labels, placements=self.placements,
            notes=['Terrain is copied unchanged; labels describe target grid regions.',
                   'Placements shift to the nearest free clear footprint.',
                   'Camera type 89 uses Side Camera records; vessels use dedicated sections.']), indent=2) + '\n')


def showcase(w, h, dat, tileset, rows):
    m = Mission('ztest_all', w, h, dat, tileset)
    for race in range(3):
        m.side(race)
    neutral = m.side(0, neutral=True)
    # Freeze showcase diplomacy and AI independently of the viewing side.
    m.trigger('localTime == 0', *(f'side({a}).setFriendlyTo({b})'
                                for a in range(3) for b in range(3) if a != b))
    for race in range(3):
        m.row_bounds = (3 + race * (h - 6) // 3, 3 + (race + 1) * (h - 6) // 3)
        band = m.row_bounds[0] + 3
        m.labels.append(f'Row {race + 1}: {RACES[race]}; columns: buildings / units / towns / orders.')
        m.labels.append('Health subrows: 100, 66, 33, 10 percent. Terrain obstacles shift cells.')
        towns = [m.town(race, race, level, w - 42 + (level % 2) * 12,
                        band + (level // 2) * 12) for level in range(4)]
        buildings = [r for r in rows if r['race'] in (race, -1) and r['kind'] in STRUCTURES]
        mobiles = [r for r in rows if r['kind'] in HP_FIELDS and r['kind'] not in STRUCTURES
                   and r['race'] in (race, -1)]
        for i, row in enumerate(buildings):
            for f, fraction in enumerate(FRACTIONS):
                m.unit(towns[0], row, 5 + i % 9 * 6, band + (i // 9 * 4 + f) * 6, fraction)
        team = m.team(race)
        for i, row in enumerate(mobiles):
            for f, fraction in enumerate(FRACTIONS):
                m.unit(team, row, 65 + i % 7 * 4, band + (i // 7 * 4 + f) * 5, fraction)
        for state, order in enumerate(ORDERS):
            t = m.team(race)
            for i, row in enumerate(mobiles):
                m.unit(t, row, w - 25 + i % 6 * 3, band + 28 + state * 3 + i // 6)
            sid = t['ScenarioID']
            prefix = f'side({race}).tactic({sid})'
            action = {'move': f'moveTo({w - 8},{min(h - 4, band + 45)})',
                      'path': 'setPath(0)', 'inverted-path': 'setInvertedPath(0)',
                      'cycle': 'setCycledPath(0)', 'inverted-cycle': 'setCycledInvertedPath(0)',
                      'locked': 'lock'}.get(order)
            if action:
                m.trigger('localTime == 1', f'{prefix}.{action}')
        # Cinematic vessels enter from outside the terrain, as in campaign maps.
        m.section('Boat', Side=race, ScenarioID=200 + race, X=12 + race * 24, Y=-2,
                  EditorX=12 + race * 24, EditorY=2, EndPosition=0, Up=0, Units=0)
        m.section('Johnka', Side=race, ScenarioID=210 + race, X=12 + race * 24,
                  Y=h + 2, Enabled=0, Up=0)
        m.groups['Side'][race].update(Start_X=towns[0]['X'], Start_Y=towns[0]['Y'],
                                     Camera0=f'{towns[0]["X"]},{towns[0]["Y"]},0')
    m.row_bounds = (0, h)
    nt = m.town(neutral, 0, 0, w // 2, h // 2)
    for i, tid in enumerate((13, 48, 66)):
        m.unit(nt, rows[tid], w // 2 + 6 * i, h // 2 + 8)
    m.sections['Paths'] = dict(Count=1, **{'0': f'0,{w - 25},10,{w - 8},{h - 10}'})
    return m


def battle(w, h, dat, tileset, rows, variant, races):
    m = Mission('ztest_battle', w, h, dat, tileset)
    m.side(races[0], computer=variant == 'cvc')
    m.side(races[1], computer=True)
    m.side(2, neutral=True)
    m.labels = ['Destroy every enemy town to win; lose every player town to lose.',
                'Reserve waves move at localTime 300 and 600; all tech IDs are enabled.',
                f'Variant: {variant}. Cinematic vessels are excluded from land combat.']
    for side, race in enumerate(races):
        x = 15 if side == 0 else w - 16
        town = m.town(side, race, 3, x, h // 2)
        m.groups['Side'][side].update(Start_X=town['X'], Start_Y=town['Y'])
        buildings = [r for r in rows if r['race'] in (race, -1) and r['kind'] in STRUCTURES]
        for i, row in enumerate(buildings):
            m.unit(town, row, x + (i % 3 - 1) * 6, h // 2 - 24 + i // 3 * 7)
        army = [r for r in rows if r['kind'] in HP_FIELDS and r['kind'] not in STRUCTURES
                and r['race'] in (race, -1)]
        for wave, time in enumerate((1, 300, 600)):
            t = m.team(side)
            for n in range(12 if variant == 'stress' else 2):
                for i, row in enumerate(army):
                    xx = 42 + wave * 8 + n * 2
                    if side:
                        xx = w - 1 - xx
                    m.unit(t, row, xx, 10 + i * 5 + n % 2)
            sid = t['ScenarioID']
            m.trigger('localTime == 0', f'side({side}).tactic({sid}).lock')
            m.trigger(f'localTime == {time}', f'side({side}).tactic({sid}).unlock',
                      f'side({side}).tactic({sid}).attackTown({101 if side == 0 else 100})')
    neutral = m.team(2)
    for i, tid in enumerate((53, 54, 55, 82)):
        m.unit(neutral, rows[tid], w // 2 + i * 3, h - 10)
    m.trigger('side(1).towns == 0', 'side(0).victory')
    m.trigger('side(0).towns == 0', 'side(0).defeat')
    return m


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True, help='Private writable game copy (ordinary Maps directory)')
    parser.add_argument('--source-mission', default='mission_14', help='Existing campaign terrain basename')
    parser.add_argument('--variant', choices=('pvc', 'cvc', 'stress'), default='pvc')
    parser.add_argument('--races', default='0,1', help='Two distinct combat races: 0 Rus, 1 Aztec, 2 Chinese')
    parser.add_argument('--force', action='store_true', help='Replace existing generated outputs')
    args = parser.parse_args()
    maps = args.game.absolute() / 'Maps'
    if not maps.is_dir() or maps.is_symlink() or args.game.is_symlink():
        parser.error('--game must have a private ordinary Maps directory')
    if not re.fullmatch(r'mission_\d+', args.source_mission, re.I):
        parser.error('--source-mission must be a campaign mission basename')
    try:
        races = tuple(int(v) for v in args.races.split(','))
        if len(races) != 2 or len(set(races)) != 2 or any(v not in range(3) for v in races):
            raise ValueError('--races requires two distinct values from 0,1,2')
        sources = {ext: find(maps, f'{args.source_mission}.{ext}') for ext in EXTENSIONS}
        source_ini = read_ini(find(maps, f'{args.source_mission}.mp'))
        terrain = sources['map'].read_bytes()
        w, h = struct.unpack('<HH', terrain[:4])
        dat = sources['dat'].read_bytes()
        if w < 180 or h < 180 or len(terrain) != 4 + 4 * w * h or len(dat) != w * h:
            raise ValueError('Template must be at least 180x180 with valid .map/.dat sizes')
        names = ('ztest_all', 'ztest_battle')
        outputs = [maps / f'{name}.{ext}' for name in names
                   for ext in (*EXTENSIONS, 'mp', 'scn', 'layout.json')]
        for path in outputs:
            if path.is_symlink() or (path.exists() and not args.force):
                raise ValueError(f'Refusing to overwrite {path}; use --force for generated regular files')
        rows = metadata()
        tileset = int(source_ini['Globals']['TileSet'])
        missions = (showcase(w, h, dat, tileset, rows),
                    battle(w, h, dat, tileset, rows, args.variant, races))
        for mission in missions:
            for ext, source in sources.items():
                shutil.copyfile(source, maps / f'{mission.name}.{ext}')
            mission.write(maps)
            print(f'{mission.name}: {len(mission.placements)} explicit objects, '
                  f'{len(mission.groups["Town"])} towns; terrain copied from {args.source_mission}')
    except (ValueError, OSError, KeyError, StopIteration, struct.error) as exc:
        parser.error(str(exc))


if __name__ == '__main__':
    main()
