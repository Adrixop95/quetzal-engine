#!/usr/bin/env python3
"""Run isolated, bounded campaign simulations twice and retain private evidence."""
import argparse
import hashlib
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import signal
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def positive(text):
    value = int(text)
    if value <= 0:
        raise argparse.ArgumentTypeError('must be positive')
    return value


def discover(game):
    maps = next((p for p in game.iterdir() if p.name.lower() == 'maps'), None)
    if maps is None or not maps.is_dir():
        raise ValueError('game directory has no Maps directory')
    missions = {}
    for path in maps.iterdir():
        match = re.fullmatch(r'mission_(\d+)\.mp', path.name, re.IGNORECASE)
        if match:
            index = int(match[1])
            if index in missions:
                raise ValueError(f'duplicate campaign mission {index}')
            missions[index] = path
    if not missions:
        raise ValueError('no campaign mission_N.mp files found')
    return maps, missions


def private_game(source, maps, destination):
    """Keep ordinary Maps/Saves directories; never expose shared writable profiles."""
    destination.mkdir()
    (destination / 'Maps').mkdir()
    (destination / 'Saves').mkdir()
    for item in source.iterdir():
        if item.name.lower() in ('maps', 'saves'):
            continue
        target = destination / item.name
        if item.is_file() and item.suffix.lower() in ('.ini', '.plr', '.cfg'):
            shutil.copyfile(item, target)
        else:
            target.symlink_to(item.resolve(), target_is_directory=item.is_dir())
    for item in maps.iterdir():
        if item.is_file():
            (destination / 'Maps' / item.name).symlink_to(item.resolve())


def run_once(args, mission, game, directory):
    directory.mkdir()
    report_path = directory / 'engine.json'
    command = ['unshare', '-rn', str(args.engine), '--headless', '--game', str(game),
               '--rules-dir', str(args.rules_dir), '--max-ticks', str(args.max_ticks),
               '--order-interval', str(args.order_interval), '--report', str(report_path)]
    if args.no_auto_orders:
        command.append('--no-auto-orders')
    if args.orders_dir:
        orders = args.orders_dir / f'mission_{mission}.orders'
        if orders.exists():
            command.extend(['--orders', str(orders)])
    command.append(str(mission))
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    result = {'command': command}
    with (directory / 'stdout.log').open('wb') as stdout, (directory / 'stderr.log').open('wb') as stderr:
        try:
            with subprocess.Popen(command, stdout=stdout, stderr=stderr, env=env,
                                  cwd=ROOT, start_new_session=True) as process:
                try:
                    result['returncode'] = process.wait(timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait()
                    result.update(status='wall_timeout', returncode=process.returncode)
                    return result
        except OSError as error:
            result.update(status='launch_error', error=str(error))
            return result
    code = result['returncode']
    if code < 0 or code >= 128:
        result['status'] = 'crash'
    elif not report_path.exists():
        result['status'] = 'isolation_or_launch_error'
    else:
        try:
            report = json.loads(report_path.read_text())
            required = {'schema', 'mission', 'status', 'ticks', 'state_hash', 'trace_hash',
                        'script_errors', 'unreachable_objectives', 'unmet_objectives', 'stalled_order_ids'}
            if not isinstance(report, dict) or not required <= report.keys() or report['schema'] != 1:
                raise ValueError('invalid engine report schema')
            if (report['status'] not in ('victory', 'defeat', 'timeout', 'error') or
                    type(report['ticks']) is not int or report['ticks'] < 0 or
                    any(not isinstance(report[key], str) or not re.fullmatch(r'[0-9a-f]{1,16}', report[key])
                        for key in ('state_hash', 'trace_hash')) or
                    any(not isinstance(report[key], list)
                        for key in ('script_errors', 'unreachable_objectives', 'unmet_objectives', 'stalled_order_ids'))):
                raise ValueError('invalid engine report values')
            result['engine'] = report
            result['status'] = report['status'] if code == 0 else 'engine_error'
        except (OSError, ValueError) as error:
            result.update(status='report_error', error=str(error))
    return result


def compare(runs):
    if len(runs) != 2 or not all(r.get('returncode') == 0 and 'engine' in r and
                                 r['engine']['status'] in ('victory', 'defeat', 'timeout') for r in runs):
        return None
    keys = ('ticks', 'status', 'state_hash', 'trace_hash')
    return all(runs[0]['engine'][key] == runs[1]['engine'][key] for key in keys)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True, help="user's private read-only game installation")
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/aztec_engine')
    parser.add_argument('--rules-dir', type=Path, default=ROOT / 'docs/data')
    parser.add_argument('--out', type=Path, help='new directory under this worktree work/')
    parser.add_argument('--missions', type=positive, nargs='+', help='default: every discovered campaign mission')
    parser.add_argument('--max-ticks', type=positive, default=43200, help='12 Hz simulation budget (default: one hour)')
    parser.add_argument('--timeout', type=positive, default=180, help='wall seconds per process')
    parser.add_argument('--order-interval', type=positive, default=120)
    parser.add_argument('--orders-dir', type=Path, help='private mission_N.orders files')
    parser.add_argument('--no-auto-orders', action='store_true')
    args = parser.parse_args(argv)
    args.game = args.game.resolve()
    args.engine = args.engine.resolve()
    args.rules_dir = args.rules_dir.resolve()
    if args.orders_dir:
        args.orders_dir = args.orders_dir.resolve()
    try:
        maps, available = discover(args.game)
        missions = sorted(set(args.missions or available))
        missing = set(missions) - available.keys()
        if missing:
            raise ValueError(f'missing requested missions: {sorted(missing)}')
        if not args.engine.is_file():
            raise ValueError(f'engine does not exist: {args.engine}')
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        output = (args.out or ROOT / 'work' / f'playthrough-{stamp}').resolve()
        private_root = (ROOT / 'work').resolve()
        if output == private_root or not output.is_relative_to(private_root):
            raise ValueError('--out must be a new directory under this worktree work/')
        if output.is_relative_to(args.game) or args.game.is_relative_to(output):
            raise ValueError('output and source game must not overlap')
        for source in (args.rules_dir, args.orders_dir):
            if source and (output.is_relative_to(source) or source.is_relative_to(output)):
                raise ValueError('output and rule/order sources must not overlap')
        output.mkdir(parents=True, exist_ok=False)
        # Pin both runs to the same inputs even when the developer rebuilds the engine.
        engine_source = args.engine
        args.engine = output / 'aztec_engine'
        shutil.copy2(engine_source, args.engine)
        shutil.copytree(args.rules_dir, output / 'rules')
        args.rules_dir = output / 'rules'
        if args.orders_dir:
            shutil.copytree(args.orders_dir, output / 'orders')
            args.orders_dir = output / 'orders'
        # Separate installations for both processes, including copied profile/configuration files.
        games = [output / f'game-{i}' for i in (1, 2)]
        for game in games:
            private_game(args.game, maps, game)
        summary = {'schema': 1, 'method': 'independent engine; no reference parity claim',
                   'engine_sha256': hashlib.sha256(args.engine.read_bytes()).hexdigest(),
                   'max_ticks': args.max_ticks, 'wall_timeout': args.timeout, 'missions': []}
        failed = False
        for mission in missions:
            runs = [run_once(args, mission, game, output / f'mission-{mission}-run-{i}')
                    for i, game in enumerate(games, 1)]
            deterministic = compare(runs)
            row = {'mission': mission, 'deterministic': deterministic, 'runs': runs}
            summary['missions'].append(row)
            failed |= deterministic is not True
            for run in runs:
                engine = run.get('engine', {})
                failed |= (run['status'] != 'victory' or bool(engine.get('script_errors')) or
                           bool(engine.get('unreachable_objectives')) or bool(engine.get('unsupported_executions')))
            (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
            print(f"mission {mission}: {runs[0]['status']} / {runs[1]['status']}; determinism={deterministic}", flush=True)
        print(f'Report: {output / "summary.json"}')
        return 1 if failed else 0
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    sys.exit(main())
