#!/usr/bin/env python3
"""Honest player-route runner and offline detectors; all run evidence stays private."""
import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import struct
import subprocess

from playthrough import private_game

ROOT = Path(__file__).resolve().parents[1]


class Detectors:
    """Consume contiguous post-tick snapshots without changing simulation state."""

    def __init__(self, *, goal=600, stuck=240, idle=1200, economy=1200, path=3):
        self.bounds = dict(goal=goal, stuck=stuck, idle=idle, economy=economy, path=path)
        self.previous = None
        self.motion = {}
        self.idle_since = {}
        self.goals = {}
        self.seen = set()
        self.anomalies = []
        self.last_income = 0

    def add(self, kind, tick, obj, reason, identity=None):
        key = kind, obj['id'] if obj else None, identity
        if key in self.seen:
            return
        self.seen.add(key)
        self.anomalies.append(dict(kind=kind, tick=tick, object=obj['id'] if obj else None,
                                   position=obj['position'] if obj else None, reason=reason))

    @staticmethod
    def near(obj, target):
        # Group formation slots may surround the requested point by one tile.
        return max(abs(a - b) for a, b in zip(obj['position'], target)) <= 48

    def feed(self, row):
        tick = row['tick']
        if self.previous is not None and tick != self.previous['tick'] + 1:
            raise ValueError('non-contiguous playthrough observations')
        if self.previous is None and tick != 0:
            raise ValueError('missing initial playthrough observation')
        objects = {o['id']: o for o in row['objects']}
        before = {o['id']: o for o in self.previous['objects']} if self.previous else {}
        if self.previous and row['income'] > self.previous['income']:
            self.last_income = tick
        if tick - self.last_income >= self.bounds['economy']:
            self.add('economy-stall', tick, None, 'No recorded harvest income within the configured bound')
        for obj in objects.values():
            oid = obj['id']
            old = before.get(oid)
            if old and old['alive'] and not obj['alive']:
                if obj['type'] in (84, 88):
                    self.add('transport-destroyed', tick, obj, 'A living transport was destroyed')
                if old['carrier']:
                    self.add('passenger-lost', tick, obj, f"Passenger died while aboard carrier {old['carrier']}")
            if old and old['cargo']:
                for passenger in set(old['cargo']) - set(obj['cargo']):
                    current = objects.get(passenger)
                    if current is None or not current['alive'] or current['carrier'] == oid:
                        self.add('passenger-lost', tick, current or dict(id=passenger, position=old['position']),
                                 f'Passenger {passenger} disappeared from carrier {oid}', passenger)
            if not obj['alive'] or obj['carrier']:
                self.goals.pop(oid, None)
                self.idle_since.pop(oid, None)
                continue
            signature = (tuple(obj['position']), obj['order'], tuple(obj['destination']), obj['target'], obj['boarding'])
            state = self.motion.get(oid)
            if state is None or state[0] != signature:
                self.motion[oid] = signature, tick
            elif (obj['order'] or obj['boarding']) and tick - state[1] >= self.bounds['stuck']:
                self.add('stuck', tick, obj, 'Position unchanged while holding an order', state[1])
            if obj['path_failures'] >= self.bounds['path']:
                self.add('repeated-path-failure', tick, obj, f"{obj['path_failures']} failed path searches or unreachable requests")
            if obj['mobile'] and not obj['order'] and not obj['boarding'] and obj['type'] not in (84, 88):
                start = self.idle_since.setdefault(oid, tick)
                if tick - start >= self.bounds['idle']:
                    self.add('idle', tick, obj, 'Mobile object left without an order', start)
            else:
                self.idle_since.pop(oid, None)
        for command in row['commands']:
            kind, ids = command['type'], command['ids']
            for oid in ids:
                obj = objects.get(oid)
                if obj is None:
                    self.add('order-ignored', tick, None, f'Order references absent object {oid}', (tick, oid))
                    continue
                if kind in (0x17, 0x51):
                    target = [command['a'], command['b']]
                    self.goals[oid] = tick, target
                    if not obj['order'] and not obj['carrier'] and not self.near(obj, target):
                        self.add('order-ignored', tick, obj, 'Movement command produced no order away from destination', tick)
                elif kind == 0x54:
                    self.goals.pop(oid, None)
                    if not (obj['boarding'] == command['a'] or obj['carrier'] == command['a']):
                        self.add('order-ignored', tick, obj, 'Board command produced no boarding or embarkation', tick)
                elif kind == 0x55:
                    old = before.get(oid)
                    if obj['type'] not in (84, 88) or (old and old['cargo'] and obj['cargo'] == old['cargo']):
                        self.add('order-ignored', tick, obj, 'Unload released no passengers; invalid selection, blocked shore or target out of range', tick)
                elif kind == 0x50:
                    self.goals.pop(oid, None)
                elif kind == 0x18 and obj['target'] != command['a']:
                    self.add('order-ignored', tick, obj, 'Attack command did not retain its target', tick)
            if kind in (0x1c, 0x52):
                prior = Counter(o['type'] for o in before.values() if o['alive'])
                after = Counter(o['type'] for o in objects.values() if o['alive'])
                prior_queue = Counter(self.previous['queue']) if self.previous else Counter()
                queued = Counter(row['queue'])
                count = sum(c['type'] in (0x1c, 0x52) and c['a'] == command['a'] for c in row['commands'])
                if after[command['a']] - prior[command['a']] + queued[command['a']] - prior_queue[command['a']] < count:
                    self.add('order-ignored', tick, objects.get(ids[0]) if ids else None,
                             'Production request was not queued or created in full', (tick, command['a']))
        for oid, (start, target) in list(self.goals.items()):
            obj = objects.get(oid)
            if obj and obj['alive'] and self.near(obj, target):
                del self.goals[oid]
            elif tick - start >= self.bounds['goal']:
                self.add('goal-timeout', tick, obj, f'Did not reach requested pixels {target} within {self.bounds["goal"]} ticks', start)
                del self.goals[oid]
        if row.get('unsupported_sites'):
            self.add('unsupported-action', tick, None, f"Mission contains {row['unsupported_sites']} unsupported action sites")
        if row['unsupported']:
            self.add('unsupported-action', tick, None, f"Mission executed {row['unsupported']} unsupported actions")
        self.previous = row


def read_observations(path, end_record=None):
    ended = False
    with path.open() as stream:
        for line in stream:
            row = json.loads(line)
            if not isinstance(row, dict):
                raise ValueError('observation must be an object')
            if ended:
                raise ValueError('observations after terminal record')
            if row.get('event') == 'end':
                if type(row.get('script_complete')) is not bool or type(row.get('failures')) is not int:
                    raise ValueError('invalid terminal record')
                ended = True
                if end_record is not None:
                    end_record.update(row)
                continue
            yield row
    if end_record is not None and not ended:
        raise ValueError('missing terminal record')


def launch(command, directory, env, timeout):
    result = dict(command=command, wall_timeout=False)
    with (directory / 'stdout.log').open('wb') as out, (directory / 'stderr.log').open('wb') as err:
        try:
            with subprocess.Popen(command, stdout=out, stderr=err, env=env, start_new_session=True) as process:
                try:
                    result['returncode'] = process.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    result.update(returncode=process.wait(), wall_timeout=True)
        except OSError as error:
            result.update(returncode=None, error=str(error))
    return result


def execute(args, output, game, mission, script, *, launcher=('unshare', '-rn')):
    """The injectable launcher is for generated test fixtures only; CLI always isolates."""
    output.mkdir()
    observations = output / 'observations.jsonl'
    # record/capture paths are script tokens, so reject whitespace instead of silently truncating.
    if any(c.isspace() for c in str(output)) or '#' in str(output):
        raise ValueError('output path must contain no whitespace or #')
    if any(line.split('#', 1)[0].strip().split()[:1] in (['record'], ['capture'], ['budget']) for line in script.splitlines()):
        raise ValueError('runner owns record and capture directives')
    (output / 'input.txt').write_text(script)
    generated = output / 'run.txt'
    generated.write_text(f'budget {args.max_ticks}\nrecord {observations}\n' + script)
    captures = output / 'captures'
    captures.mkdir()

    def command_for(directory, input_path, periodic):
        command = [*launcher, str(args.engine), '--game', str(game), str(mission), '--rules-dir', str(args.rules_dir),
                   '--profile', 'faithful', '--script', str(input_path), '--frames', str(args.max_ticks + 1),
                   '--size', '800x600', '--software', '--no-audio', '--no-light', '--no-fog',
                   '--ui-trace', str(directory / 'ui.jsonl')]
        if periodic:
            command += ['--capture-dir', str(captures), '--capture-every', str(args.capture_every)]
        if args.mode == 'xvfb':
            command = ['xvfb-run', '-a', '-e', str(directory / 'xvfb.log'), *command]
        return command

    env = dict(os.environ, SDL_AUDIODRIVER='dummy', AZTEC_GAME=str(game), AZTEC_USER_DIR=str(output / 'profile'))
    if args.mode == 'headless':
        env['SDL_VIDEODRIVER'] = 'dummy'
    else:
        env['SDL_VIDEODRIVER'] = 'x11'
    result = launch(command_for(output, generated, args.capture_every), output, env, args.timeout)
    detector = Detectors(goal=args.goal_ticks, stuck=args.stuck_ticks, idle=args.idle_ticks,
                         economy=args.economy_ticks, path=args.path_failures)
    hashes = {}
    end_record = {}
    try:
        for row in read_observations(observations, end_record):
            detector.feed(row)
            hashes[row['tick']] = row['hash']
    except (OSError, ValueError, KeyError, TypeError) as error:
        detector.add('invalid-observations', detector.previous['tick'] if detector.previous else 0, None, str(error))
    last = detector.previous or dict(tick=0, outcome=0)
    if end_record and end_record['tick'] != last['tick']:
        detector.add('invalid-observations', last['tick'], None, 'Terminal record disagrees with final observed tick')
    final_objects = {o['id']: o for o in last.get('objects', [])}
    for oid, (start, target) in detector.goals.items():
        if last['outcome'] != 0:
            continue  # A natural terminal objective stops simulation before remaining orders can finish.
        detector.add('goal-incomplete', last['tick'], final_objects.get(oid),
                     f'Scenario ended before the requested pixels {target} were reached', start)
    for stream in ('stdout.log', 'stderr.log'):
        for line in (output / stream).read_text(errors='replace').splitlines():
            if re.search(r'\b(warn(?:ing)?|unsupported|FAIL|error|placeholder rules)\b', line, re.IGNORECASE):
                match = re.search(r'\btick (\d+)\b', line)
                detector.add('engine-warning', int(match[1]) if match else last['tick'], None, line, line)
    if not result['wall_timeout'] and result['returncode'] != 0:
        detector.add('process-failure', last['tick'], None, f"Engine return code {result['returncode']}: {result.get('error', '')}")
    outcome = {1: 'won', 2: 'lost'}.get(last['outcome'], 'timeout')
    if outcome in ('won', 'lost') and not end_record.get('script_complete'):
        detector.add('scenario-incomplete', last['tick'], None, 'Route ended before all steps/checkpoints executed')
    if detector.anomalies or (outcome == 'timeout' and last['tick'] < args.max_ticks and not result['wall_timeout']):
        outcome = 'scenario-failed'
    if result['wall_timeout']:
        outcome = 'timeout'
    report = dict(schema=1, outcome=outcome, ticks=last['tick'], anomalies=detector.anomalies,
                  process=result, scenario_sha256=hashlib.sha256(script.encode()).hexdigest(),
                  engine_sha256=hashlib.sha256(args.engine.read_bytes()).hexdigest(),
                  bounds=detector.bounds, mode=args.mode, capture_replay=None, script_complete=end_record.get('script_complete', False),
                  engine_outcome={0: 'running', 1: 'won', 2: 'lost'}[last['outcome']],
                  pending_goals=[dict(object=oid, since=start, destination=target)
                                 for oid, (start, target) in detector.goals.items()])
    if args.capture_every and detector.anomalies and hashes:
        replay = output / 'anomaly-replay'
        replay.mkdir()
        ticks = sorted({a['tick'] for a in detector.anomalies if a['tick'] > 0})
        replay_input = replay / 'run.txt'
        replay_input.write_text(f'budget {args.max_ticks}\nrecord {replay / "observations.jsonl"}\n' + ''.join(
            f'capture {tick} {captures / f"anomaly-{tick}.png"}\n' for tick in ticks) + script)
        replay_env = dict(env, AZTEC_USER_DIR=str(replay / 'profile'))
        replay_result = launch(command_for(replay, replay_input, False), replay, replay_env, args.timeout)
        try:
            repeated = {r['tick']: r['hash'] for r in read_observations(replay / 'observations.jsonl')}
            equal = hashes == repeated and replay_result['returncode'] == result['returncode']
        except (OSError, ValueError, KeyError):
            equal = False
        report['capture_replay'] = dict(verified=equal, process=replay_result)
        for anomaly in detector.anomalies:
            capture = captures / f'anomaly-{anomaly["tick"]}.png'
            if equal and capture.is_file():
                anomaly['capture'] = str(capture.relative_to(output))
        if not equal or any(a['tick'] > 0 and 'capture' not in a for a in detector.anomalies):
            detector.add('capture-failure', last['tick'], None, 'Anomaly replay or exact tick capture could not be verified')
            report['outcome'] = 'scenario-failed'
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    markdown = [f'# Playthrough: {report["outcome"]}', '', f'Ticks: {last["tick"]}', '',
                '| Tick | Object | Position (pixels) | Kind | Reason | Capture |', '| --- | --- | --- | --- | --- | --- |']
    for a in report['anomalies']:
        reason = a['reason'].replace('|', '\\|').replace('\n', ' ')
        capture = f'[frame]({a["capture"]})' if 'capture' in a else ''
        markdown.append(f'| {a["tick"]} | {a["object"]} | {a["position"]} | {a["kind"]} | {reason} | {capture} |')
    (output / 'report.md').write_text('\n'.join(markdown) + '\n')
    return report


def demo_game(destination):
    """Generate invented terrain/resources; no installation or executable is read."""
    destination.mkdir()
    (destination / 'Res').mkdir()
    maps = destination / 'Maps'
    maps.mkdir()
    for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
        (destination / 'Res' / f'{name}.xbe').write_bytes(bytes(4))
    base = maps / 'demo'
    base.with_suffix('.mp').write_text(
        '[Globals]\nMapSize=32,24\nSides=1\nTeams=1\nTowns=1\nBoats=1\n'
        '[Side0]\nStart_X=8\nStart_Y=10\nStartMoney=10000\n'
        '[Town0]\nSide=0\nX=5\nY=6\nType=1\nRace=0\n'
        '[Team0]\nSide=0\nUnits=1\nUnit0=3,12,0,100\n'
        '[Boat0]\nSide=0\nX=16\nY=12\n')
    base.with_suffix('.map').write_bytes(struct.pack('<HH', 32, 24) + bytes(32 * 24 * 4))
    # Land is bit 1, water is 0; solid wall uses all class-blocking bits.
    base.with_suffix('.dat').write_bytes(bytes(
        0 if 16 <= x <= 21 else 255 if x == 10 and y not in (11, 12) else 2
        for y in range(24) for x in range(32)))
    base.with_suffix('.rm').write_bytes(struct.pack('<HH', 32, 24) + bytes([255, 0]) * (32 * 24))
    base.with_suffix('.scn').write_text(
        'trigger(0) conditions side(local).unitsInRect(25,11,27,13) == 3 actions side(local).victory end\n')
    return destination


def positive(text):
    number = int(text)
    if number < 1:
        raise argparse.ArgumentTypeError('must be positive')
    return number


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument('--scenario', type=Path, help='private player script; existing low-level commands also work')
    source.add_argument('--demo', action='store_true', help='generate and run the public synthetic loop')
    parser.add_argument('--game', type=Path)
    parser.add_argument('--mission', default='1')
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/aztec_engine')
    parser.add_argument('--rules-dir', type=Path, default=ROOT / 'docs/data')
    parser.add_argument('--out', type=Path)
    parser.add_argument('--max-ticks', type=positive, default=6000)
    parser.add_argument('--timeout', type=positive, default=120)
    parser.add_argument('--capture-every', type=positive)
    parser.add_argument('--mode', choices=('headless', 'xvfb'), default='headless')
    for flag, default in (('goal-ticks', 600), ('stuck-ticks', 240), ('idle-ticks', 1200), ('economy-ticks', 1200), ('path-failures', 3)):
        parser.add_argument('--' + flag, type=positive, default=default)
    args = parser.parse_args(argv)
    try:
        args.engine = args.engine.resolve(strict=True)
        args.rules_dir = args.rules_dir.resolve(strict=True)
        if args.max_ticks > 10000000:
            raise ValueError('tick limit must be at most 10000000')
        if args.capture_every and args.capture_every > 7200:
            raise ValueError('capture interval must be at most 7200 ticks')
        output = (args.out or ROOT / 'work/playthrough/reports' / datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')).resolve()
        if not output.is_relative_to(ROOT / 'work') or output.exists():
            raise ValueError('output must be a new directory under this worktree work/')
        output.parent.mkdir(parents=True, exist_ok=True)
        staging = output.parent / (output.name + '-inputs')
        staging.mkdir()
        if args.demo:
            game = demo_game(staging / 'game')
            mission = 'demo'
            script = (ROOT / 'tests/playtest/playthrough_demo.txt').read_text()
        else:
            if args.game is None:
                raise ValueError('--scenario requires --game')
            source_game = args.game.resolve(strict=True)
            maps = next(p for p in source_game.iterdir() if p.name.lower() == 'maps')
            game = staging / 'game'
            private_game(source_game, maps, game)
            mission = args.mission
            script = args.scenario.read_text()
        report = execute(args, output, game, mission, script)
        print(f'{report["outcome"]}: {report["ticks"]} ticks; report: {output / "report.json"}')
        return 0 if report['outcome'] == 'won' and not report['anomalies'] else 1
    except (OSError, ValueError, StopIteration) as error:
        parser.error(str(error))
