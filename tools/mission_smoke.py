#!/usr/bin/env python3
"""Smoke all campaign missions; retain original-data diagnostics only in ignored output."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys
import time

# Also support python -I, as used by the repository's other developer tools.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from playthrough import CAMPAIGN, ROOT, positive, private_game

# Native codes from docs/script_runtime_gap_list.md, never the independent enums.
ACTION_NAMES = '''SideVictory SideDefeat TacticAttackTown TacticMoveTo TacticJoinTactic
TimerSet TimerStop TimerStart TimerKill TacticSetPath TacticSetInvertedPath
TacticSetCycledPath TacticSetCycledInvertedPath BoatActivate SubmarineGo JohnkaEnable
JohnkaDisable SideBuildCommand SideMoveCommand SideCameraActivate CenterView
SideSetFriendlyTo SideSetNeutralTo SideSetEnemyTo TacticGiftToSide PrintString
TacticLock TacticUnlock SideAddMoney SideTakeMoney TacticKill TriggerDisable
TriggerEnable SideCameraDeactivate'''.split()
CONDITION_NAMES = {
    'Always': 0x00, 'SideMoney': 0x01, 'SideTactics': 0x02, 'SideTowns': 0x03,
    'SideEnemies': 0x04, 'SideUnits': 0x05, 'SideUnitsOfId': 0x06,
    'LocalTime': 0x07, 'GlobalTime': 0x08, 'Timer': 0x09,
    'TacticIdle': 0x0a, 'TacticAttacked': 0x0b,
    'IsFriendly': 0x0e, 'IsNeutral': 0x0f, 'IsEnemy': 0x10,
    'TacticValid': 0x11, 'TacticLocked': 0x12,
    'SideUnitsInRect': 0x13, 'SideUnitsOfUGIInRect': 0x14, 'DefenceValid': 0x15,
}
SANITIZER = re.compile(r'AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|'
                       r'MemorySanitizer|ThreadSanitizer|runtime error:')


def gap_catalog(path):
    """Read current dispositions instead of copying a second runtime gap policy."""
    section = None
    catalog = {}
    for line in path.read_text(encoding='utf-8').splitlines():
        if line.startswith('## Actions'):
            section = 'action'
        elif line.startswith('## Conditions'):
            section = 'condition'
        elif line.startswith('## '):
            section = None
        if section is None or not line.startswith('| '):
            continue
        fields = [field.strip() for field in line.split('|')[1:-1]]
        if len(fields) != 3 or not re.fullmatch(r'[0-9a-f]{2}(, [0-9a-f]{2})*', fields[0]):
            continue
        note = fields[2]
        disposition = ('unsupported' if 'Unsupported execution' in note else
                       'approximate' if re.search(r'\bG\b', note) else 'implemented')
        for code in fields[0].split(', '):
            catalog[section, int(code, 16)] = dict(code=code, disposition=disposition, note=note)
    names = {'action': dict(zip(ACTION_NAMES, range(len(ACTION_NAMES)))), 'condition': CONDITION_NAMES}
    result = {}
    for section, codes in names.items():
        for name, code in codes.items():
            if (section, code) not in catalog:
                raise ValueError(f'gap list lacks {section} code {code:02x}')
            result[section, name] = catalog[section, code]
    result['action', 'PrintStringR'] = dict(code=None, disposition='intentional_noop',
                                          note='Parser discards this alternate print form; no separate native code.')
    return result


def script_gaps(inventory, catalog):
    sites = []
    for site in inventory:
        key = site['section'], site['kind']
        if key not in catalog:
            raise ValueError(f'unknown script inventory kind: {key}')
        gap = catalog[key]
        if gap['disposition'] in ('unsupported', 'approximate', 'intentional_noop'):
            sites.append(dict(site, **gap))
    return sites


def private_output(path):
    output = path.absolute().resolve()
    # Reject symlinked private roots that could redirect data into the tracked tree.
    for name in ('work', 'build'):
        root = ROOT / name
        if root.resolve() == root and output != root and output.is_relative_to(root):
            return output
    raise ValueError('--output must be a fresh directory inside this worktree work/ or build/')


def configured_game(args):
    command = [str(args.engine), '--headless', '--print-game-dir']
    if args.settings:
        command += ['--settings', str(args.settings)]
    if args.game:
        command += ['--game', str(args.game)]
    result = subprocess.run(command, capture_output=True, text=True, timeout=15,
                            env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'))
    if result.returncode:
        raise ValueError('cannot resolve game configuration: ' + result.stdout + result.stderr)
    # The existing headless serializer escapes each non-ASCII byte as a JSON
    # U+00xx character. Recover filesystem bytes before interpreting the path.
    path = os.fsdecode(json.loads(result.stdout)['game'].encode('latin1'))
    return Path(path).resolve()


def run_mission(args, mission, source, maps, directory, catalog):
    directory.mkdir()
    game = directory / 'game'
    private_game(source, maps, game)
    report_path = directory / 'engine.json'
    command = ['timeout', '-s', 'KILL', str(args.timeout), 'unshare', '-rn', str(args.engine),
               '--headless', '--game', str(game), '--rules-dir', str(args.rules_dir),
               '--profile', 'faithful', '--no-auto-orders', '--smoke-diagnostics',
               '--max-ticks', str(args.ticks), '--report', str(report_path), str(mission)]
    # A fresh explicit settings file prevents personal settings from affecting isolated runs.
    settings = directory / 'settings.ini'
    settings.write_text('', encoding='ascii')
    command[command.index('--profile'):command.index('--profile')] = ['--settings', str(settings)]
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy',
               AZTEC_GAME=str(game), AZTEC_USER_DIR=str(directory / 'profile'))
    # Environment-driven original/runtime input automation must never leak into the smoke.
    env = {key: value for key, value in env.items() if not key.startswith('RT_')}
    row = dict(mission=mission, status='launch_error', command=command, returncode=None,
               sanitizer_build=args.sanitizer_build, sanitizer_findings=[], engine=None)
    start = time.perf_counter()
    with (directory / 'stdout.log').open('wb') as stdout, (directory / 'stderr.log').open('wb') as stderr:
        try:
            with subprocess.Popen(command, cwd=game, env=env, stdout=stdout, stderr=stderr,
                                  start_new_session=True) as process:
                try:
                    code = process.wait(timeout=args.timeout + 10)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
                    code = -9
            row['returncode'] = code
            row['status'] = ('wall_timeout' if code in (124, 137, -9) else
                             'crash' if code < 0 or code >= 128 else
                             'isolation_or_launch_error')
        except OSError as error:
            row['error'] = str(error)
    row['wall_seconds'] = time.perf_counter() - start
    for name in ('stdout.log', 'stderr.log'):
        for line in (directory / name).read_text(encoding='utf-8', errors='replace').splitlines():
            if SANITIZER.search(line):
                row['sanitizer_findings'].append(line)
    if report_path.exists():
        try:
            report = json.loads(report_path.read_text(encoding='utf-8'))
            smoke = report['smoke']
            if report['schema'] != 1 or report['mission'] != str(mission):
                raise ValueError('engine report identity mismatch')
            if (type(report['ticks']) is not int or not 0 <= report['ticks'] <= args.ticks or
                    type(smoke['loaded']) is not bool or
                    not isinstance(smoke['unresolved_descriptors'], list)):
                raise ValueError('invalid smoke report values')
            row['engine'] = report
            row['script_gaps'] = script_gaps(smoke['script_inventory'], catalog)
            row['mean_tick_seconds'] = smoke['tick_seconds'] / report['ticks'] if report['ticks'] else None
            if row['returncode'] == 0 and report['status'] in ('timeout', 'victory', 'defeat') and smoke['loaded']:
                row['status'] = 'passed'
            elif row['status'] not in ('crash', 'wall_timeout'):
                row['status'] = 'runtime_error' if smoke['loaded'] else 'load_error'
            if 'missing mission script' in report['script_errors']:
                row['status'] = 'load_error'
        except (OSError, ValueError, KeyError, TypeError) as error:
            row.update(status='report_error', error=str(error))
    if row['sanitizer_findings']:
        row['status'] = 'sanitizer_failure'
    (directory / 'result.json').write_text(json.dumps(row, indent=2) + '\n', encoding='utf-8')
    return row


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/aztec_engine')
    parser.add_argument('--game', type=Path, help='override normal engine game configuration')
    parser.add_argument('--settings', type=Path, help='use an existing engine settings.ini')
    parser.add_argument('--rules-dir', type=Path, default=ROOT / 'docs/data')
    parser.add_argument('--ticks', type=positive, default=720)
    parser.add_argument('--timeout', type=positive, default=120, help='hard wall seconds per mission')
    parser.add_argument('--sanitizer-build', action='store_true', help='record that the selected binary is instrumented')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args(argv)
    try:
        args.engine = args.engine.resolve()
        args.rules_dir = args.rules_dir.resolve()
        source = configured_game(args)
        if not source.is_dir():
            print('SKIP: configured game data directory is absent; supply --game or AZTEC_GAME.')
            return 0
        maps = next((p for p in source.iterdir() if p.name.lower() == 'maps' and p.is_dir()), None)
        if maps is None:
            print('SKIP: configured game data is absent (no Maps directory).')
            return 0
        if sys.platform != 'linux' or not shutil.which('unshare') or not shutil.which('timeout'):
            raise ValueError('live smoke requires Linux unshare and timeout; isolation cannot be bypassed')
        catalog = gap_catalog(ROOT / 'docs/script_runtime_gap_list.md')
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        output = private_output(args.output or ROOT / 'work' / f'mission-smoke-{stamp}')
        for path in (source, args.rules_dir, args.engine):
            if output.is_relative_to(path) or path.is_relative_to(output):
                raise ValueError('output must not overlap installation, rules or engine inputs')
        output.mkdir(parents=True, exist_ok=False)
        manifest = dict(schema=1, ticks_requested=args.ticks, sanitizer_build=args.sanitizer_build,
                        sanitizer_options={key: os.environ.get(key) for key in
                                           ('ASAN_OPTIONS', 'UBSAN_OPTIONS', 'LSAN_OPTIONS', 'TSAN_OPTIONS')},
                        source=str(source), engine=str(args.engine),
                        engine_sha256=hashlib.sha256(args.engine.read_bytes()).hexdigest(),
                        gap_list_sha256=hashlib.sha256((ROOT / 'docs/script_runtime_gap_list.md').read_bytes()).hexdigest(),
                        missions=[])
        for mission in CAMPAIGN:
            directory = output / f'mission_{mission}'
            try:
                row = run_mission(args, mission, source, maps, directory, catalog)
            except (OSError, ValueError) as error:
                row = dict(mission=mission, status='setup_error', error=str(error))
            manifest['missions'].append(row)
            # Per-mission details stay private, including in unattended CI logs.
            (output / 'report.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        passed = sum(row['status'] == 'passed' for row in manifest['missions'])
        print(f'Smoke: {passed}/15 passed; {15 - passed} failed. Private report: {output / "report.json"}')
        return int(passed != 15)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print(f'mission_smoke: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
