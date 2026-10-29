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
CAMPAIGN = range(1, 16)


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


def run_once(args, mission, game, directory, *, orders=None, save_tick=None, resume=None):
    directory.mkdir()
    report_path = directory / 'engine.json'
    command = ['unshare', '-rn', str(args.engine), '--headless', '--game', str(game),
               '--rules-dir', str(args.rules_dir), '--max-ticks', str(args.max_ticks),
               '--order-interval', str(args.order_interval), '--report', str(report_path)]
    hashes = directory / 'ticks.txt'
    command.extend(['--tick-hashes', str(hashes)])
    if save_tick is not None:
        command.extend(['--save-at', str(save_tick), '--save', str(directory / 'midmission.azs')])
    if resume is not None:
        command.extend(['--resume', str(resume)])
    if orders is not None:
        command.extend(['--orders', str(orders)])
    if args.no_auto_orders:
        command.append('--no-auto-orders')
    if args.orders_dir and orders is None:
        orders = args.orders_dir / f'mission_{mission}.orders'
        if orders.exists():
            command.extend(['--orders', str(orders)])
    command.append(str(mission))
    env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', AZTEC_GAME=str(game))
    result = {'command': command, 'tick_hashes': str(hashes)}
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
                        'script_errors', 'unsupported_actions', 'unsupported_executions',
                        'unreachable_objectives', 'unmet_objectives', 'stalled_order_ids'}
            if not isinstance(report, dict) or not required <= report.keys() or report['schema'] != 1 or report['mission'] != str(mission):
                raise ValueError('invalid engine report schema')
            if (report['status'] not in ('victory', 'defeat', 'timeout', 'error') or
                    type(report['ticks']) is not int or report['ticks'] < 0 or
                    type(report['unsupported_executions']) is not int or report['unsupported_executions'] < 0 or
                    any(not isinstance(report[key], str) or not re.fullmatch(r'[0-9a-f]{1,16}', report[key])
                        for key in ('state_hash', 'trace_hash')) or
                    any(not isinstance(report[key], list)
                        for key in ('script_errors', 'unsupported_actions', 'unreachable_objectives', 'unmet_objectives', 'stalled_order_ids'))):
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


def read_hashes(path):
    """Reject missing, truncated, duplicate or non-contiguous transcripts."""
    rows = []
    with Path(path).open() as stream:
        for line in stream:
            fields = line.split()
            if (len(fields) != 2 or not fields[0].isdigit() or
                    not re.fullmatch(r'[0-9a-f]{1,16}', fields[1])):
                raise ValueError('invalid tick hash row')
            tick = int(fields[0])
            if rows and tick != rows[-1][0] + 1:
                raise ValueError('non-contiguous tick hashes')
            rows.append((tick, fields[1]))
    if not rows:
        raise ValueError('empty tick hashes')
    return rows


def paired_hashes(first, second, start=0):
    """Compare every tick, including the loaded save boundary, with first divergence."""
    if not all(r.get('returncode') == 0 and r.get('engine', {}).get('status') in
               ('victory', 'defeat', 'timeout') for r in (first, second)):
        return {'equal': None, 'error': 'run did not complete'}
    try:
        a, b = (read_hashes(r['tick_hashes']) for r in (first, second))
        for run, rows in ((first, a), (second, b)):
            if rows[-1][0] != run['engine']['ticks'] or rows[-1][1] != run['engine']['state_hash']:
                raise ValueError('transcript does not match terminal report')
        if a[0][0] != 0 or b[0][0] != start or start >= a[-1][0]:
            raise ValueError('missing initial boundary or no continuation ticks')
        a = a[start:]
        for left, right in zip(a, b):
            if left != right:
                return {'equal': False, 'first_mismatch_tick': min(left[0], right[0])}
        if len(a) != len(b):
            return {'equal': False, 'first_mismatch_tick': min(a[-1][0], b[-1][0]) + 1}
        return {'equal': True, 'first_tick': start, 'last_tick': a[-1][0], 'ticks_compared': len(a)}
    except (KeyError, OSError, ValueError) as error:
        return {'equal': None, 'error': str(error)}


def scenarios(path, missions, max_ticks):
    if path is None:
        return [dict(id=f'mission-{m}', mission=m, route='Unvalidated automatic exercising policy',
                     expected_outcome='victory', save_tick=max(1, max_ticks // 2)) for m in missions]
    rows = json.loads(path.read_text())
    if not isinstance(rows, list) or not rows:
        raise ValueError('scenarios must be a nonempty JSON list')
    ids = set()
    for row in rows:
        if not isinstance(row, dict) or set(row) - {'id', 'mission', 'route', 'expected_outcome', 'save_tick', 'orders'}:
            raise ValueError('invalid scenario fields')
        if (not isinstance(row.get('id'), str) or not re.fullmatch(r'[a-zA-Z0-9_-]+', row['id']) or
                row['id'] in ids or type(row.get('mission')) is not int or row['mission'] not in CAMPAIGN or
                not isinstance(row.get('route'), str) or not row['route'].strip() or
                row.get('expected_outcome') not in ('victory', 'defeat') or
                type(row.get('save_tick')) is not int or not 0 < row['save_tick'] < max_ticks):
            raise ValueError('invalid scenario identity, route, outcome or save tick')
        ids.add(row['id'])
        if 'orders' in row:
            if not isinstance(row['orders'], str):
                raise ValueError('scenario orders must be a path')
            orders = (path.parent / row['orders']).resolve()
            if not orders.is_file():
                raise ValueError(f'missing scenario orders: {orders}')
            row['orders'] = str(orders)
    selected = [row for row in rows if row['mission'] in missions]
    if not selected:
        raise ValueError('no scenarios match selected missions')
    return selected


def mission_matrix(available, selected, rows):
    return [dict(mission=m, availability='present' if m in available else 'missing',
                 selected=m in selected, scenarios=[r for r in rows if r['mission'] == m],
                 interactive_acceptance='pending', progression='pending', defeat_retry='pending') for m in CAMPAIGN]


def write_matrix(output, matrix):
    lines = ['# Campaign coverage matrix', '',
             'Engine consistency evidence only. Interactive controls, defeat/retry and final progression remain pending.', '',
             '| Mission | Data | Scenario / route | Expected / observed | Paired ticks | Mid-mission save | Diagnostics |',
             '| --- | --- | --- | --- | --- | --- | --- |']
    def cell(value):
        return str(value).replace('|', '/').replace('\n', ' ')
    for mission in matrix:
        for row in mission['scenarios'] or [{}]:
            observed = ' / '.join(r['status'] for r in row.get('runs', [])) or 'not run'
            diagnostics = [r.get('engine', {}).get(key, []) for r in row.get('runs', [])
                           for key in ('script_errors', 'unsupported_actions', 'unreachable_objectives', 'unmet_objectives', 'stalled_order_ids')]
            values = [mission['mission'], mission['availability'], row.get('route', 'No completion route validated'),
                      row.get('expected_outcome', 'victory') + ' / ' + observed,
                      row.get('paired_ticks', {}).get('equal'), row.get('save_continuation', {}).get('equal'),
                      json.dumps([d for d in diagnostics if d])]
            lines.append('| ' + ' | '.join(map(cell, values)) + ' |')
    (output / 'matrix.md').write_text('\n'.join(lines) + '\n')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True, help="user's private read-only game installation")
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/aztec_engine')
    parser.add_argument('--rules-dir', type=Path, default=ROOT / 'docs/data')
    parser.add_argument('--out', type=Path, help='new directory under this worktree work/')
    parser.add_argument('--missions', type=positive, nargs='+', help='default: missions 1 through 15')
    parser.add_argument('--max-ticks', type=positive, default=43200, help='12 Hz simulation budget (default: one hour)')
    parser.add_argument('--timeout', type=positive, default=180, help='wall seconds per process')
    parser.add_argument('--order-interval', type=positive, default=120)
    parser.add_argument('--orders-dir', type=Path, help='private mission_N.orders files')
    parser.add_argument('--scenarios', type=Path, help='private JSON scenario list with routes, outcomes and save ticks')
    parser.add_argument('--no-auto-orders', action='store_true')
    args = parser.parse_args(argv)
    args.game = args.game.resolve()
    args.engine = args.engine.resolve()
    args.rules_dir = args.rules_dir.resolve()
    if args.orders_dir:
        args.orders_dir = args.orders_dir.resolve()
    try:
        maps, available = discover(args.game)
        missions = sorted(set(args.missions or CAMPAIGN))
        if set(missions) - set(CAMPAIGN) or args.max_ticks < 2:
            raise ValueError('campaign missions must be 1 through 15; max-ticks must be at least 2')
        planned = scenarios(args.scenarios.resolve() if args.scenarios else None, missions, args.max_ticks)
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
        # Snapshot scenario orders alongside the plan; source files remain private.
        for row in planned:
            if 'orders' in row:
                target = output / (row['id'] + '.orders')
                shutil.copyfile(row['orders'], target)
                row['orders'] = str(target)
        (output / 'scenarios.json').write_text(json.dumps(planned, indent=2) + '\n')
        # Separate installations for both processes, including copied profile/configuration files.
        games = [output / f'game-{i}' for i in (1, 2)]
        for game in games:
            private_game(args.game, maps, game)
        summary = {'schema': 2, 'method': 'independent engine; no reference parity claim',
                   'engine_sha256': hashlib.sha256(args.engine.read_bytes()).hexdigest(),
                   'max_ticks': args.max_ticks, 'wall_timeout': args.timeout, 'missions': [],
                   'acceptance': 'pending interactive controls, defeat/retry and final progression'}
        failed = bool(set(missions) - available.keys())
        failed |= any(not any(s['mission'] == m for s in planned) for m in missions)
        for scenario in planned:
            mission = scenario['mission']
            row = dict(scenario, runs=[])
            summary['missions'].append(row)
            if mission in available:
                orders = Path(scenario['orders']) if 'orders' in scenario else None
                runs = [run_once(args, mission, game, output / f"{scenario['id']}-run-{i}",
                                 orders=orders, save_tick=scenario['save_tick'])
                        for i, game in enumerate(games, 1)]
                row['runs'] = runs
                row['paired_ticks'] = paired_hashes(*runs)
                terminal_match, tick_match = compare(runs), row['paired_ticks']['equal']
                row['deterministic'] = (None if terminal_match is None or tick_match is None
                                        else terminal_match and tick_match)
                saved = output / f"{scenario['id']}-run-1" / 'midmission.azs'
                if saved.is_file() and runs[0].get('engine', {}).get('ticks', 0) > scenario['save_tick']:
                    resumed = run_once(args, mission, games[1], output / f"{scenario['id']}-resume",
                                       orders=orders, resume=saved)
                    row['resumed_run'] = resumed
                    row['save_continuation'] = paired_hashes(runs[0], resumed, scenario['save_tick'])
                    if row['save_continuation']['equal'] is True:
                        row['save_continuation']['equal'] = all(
                            runs[0]['engine'][k] == resumed['engine'][k] for k in ('ticks', 'status', 'state_hash'))
                else:
                    row['save_continuation'] = {'equal': None, 'error': 'no completed baseline with a mid-mission save'}
                row['passed'] = (row['deterministic'] is True and row['save_continuation']['equal'] is True and
                                 all(r['status'] == scenario['expected_outcome'] and
                                     not any(r.get('engine', {}).get(k) for k in
                                             ('script_errors', 'unreachable_objectives', 'unsupported_executions'))
                                     for r in runs))
                print(f"{scenario['id']}: {' / '.join(r['status'] for r in runs)}; passed={row['passed']}", flush=True)
            else:
                row.update(passed=False, error='missing mission data')
            failed |= not row['passed']
            summary['matrix'] = mission_matrix(available, missions, summary['missions'])
            (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
            write_matrix(output, summary['matrix'])
        # Emit a complete matrix even when the selected plan contains no scenarios.
        summary['matrix'] = mission_matrix(available, missions, summary['missions'])
        (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        write_matrix(output, summary['matrix'])
        print(f'Report: {output / "summary.json"}')
        return 1 if failed else 0
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    sys.exit(main())
