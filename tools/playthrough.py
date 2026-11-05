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
            validate_action_observations(report)
            result['engine'] = report
            result['status'] = report['status'] if code == 0 else 'engine_error'
        except (OSError, ValueError) as error:
            result.update(status='report_error', error=str(error))
    return result


def validate_action_observations(report):
    """Reject incomplete/contradictory execution evidence before labelling sites dormant."""
    rows = report.get('action_observations')
    if not isinstance(rows, list):
        raise ValueError('missing action observations')
    seen = set()
    for row in rows:
        integers = ('trigger_index', 'action_index', 'line', 'executions', 'first_tick', 'last_tick')
        if (not isinstance(row, dict) or
                any(type(row.get(k)) is not int or row[k] < 0 for k in integers) or
                type(row.get('trigger')) is not int or
                not isinstance(row.get('kind'), str) or not re.fullmatch(r'[A-Za-z]+', row['kind']) or
                any(type(row.get(k)) is not bool for k in ('supported', 'enabled')) or
                not isinstance(row.get('false_condition_lines'), list) or
                any(type(line) is not int or line < 0 for line in row['false_condition_lines'])):
            raise ValueError('invalid action observation')
        identity = (row['trigger_index'], row['action_index'])
        if identity in seen:
            raise ValueError('duplicate action observation')
        seen.add(identity)
        count, first, last = (row[k] for k in ('executions', 'first_tick', 'last_tick'))
        if ((count == 0 and (first or last)) or
                (count > 0 and not (1 <= first <= last <= report['ticks'] and count <= last - first + 1))):
            raise ValueError('invalid action execution history')
    if sum(r['executions'] for r in rows if not r['supported']) != report['unsupported_executions']:
        raise ValueError('action observations disagree with unsupported execution count')


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
        if not isinstance(row, dict) or set(row) - {'id', 'mission', 'route', 'expected_outcome', 'save_tick', 'orders', 'steps', 'checkpoints'}:
            raise ValueError('invalid scenario fields')
        if (not isinstance(row.get('id'), str) or not re.fullmatch(r'[a-zA-Z0-9_-]+', row['id']) or
                row['id'] in ids or type(row.get('mission')) is not int or row['mission'] not in CAMPAIGN or
                not isinstance(row.get('route'), str) or not row['route'].strip() or
                row.get('expected_outcome') not in ('victory', 'defeat') or
                type(row.get('save_tick')) is not int or not 0 < row['save_tick'] < max_ticks):
            raise ValueError('invalid scenario identity, route, outcome or save tick')
        ids.add(row['id'])
        validate_route(row, max_ticks)
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


def validate_route(row, max_ticks):
    """Only ordinary movement orders are currently supported by the engine driver."""
    if 'steps' in row and 'orders' in row:
        raise ValueError('use route steps or an orders file, not both')
    previous = 0
    if not isinstance(row.get('steps', []), list):
        raise ValueError('route steps must be a list')
    for step in row.get('steps', []):
        if (not isinstance(step, dict) or set(step) != {'tick', 'action', 'side', 'x', 'y', 'ids'} or
                any(type(step[k]) is not int for k in ('tick', 'side', 'x', 'y')) or
                not max(1, previous) <= step['tick'] <= max_ticks or
                not 0 <= step['side'] < 8 or not all(0 <= step[k] <= 2147483647 for k in ('x', 'y')) or
                step['action'] not in ('move', 'attack-move') or
                not isinstance(step['ids'], list) or
                any(type(i) is not int or not 1 <= i <= 2147483647 for i in step['ids']) or
                len(set(step['ids'])) != len(step['ids'])):
            raise ValueError('invalid or unsupported route step')
        previous = step['tick']
    checkpoints = row.get('checkpoints', [])
    if not isinstance(checkpoints, list):
        raise ValueError('route checkpoints must be a list')
    names, sites = set(), set()
    for checkpoint in checkpoints:
        if (not isinstance(checkpoint, dict) or
                set(checkpoint) != {'id', 'trigger_index', 'action_index', 'kind'} or
                not isinstance(checkpoint['id'], str) or not re.fullmatch(r'[a-z0-9_-]+', checkpoint['id']) or
                checkpoint['id'] in names or
                any(type(checkpoint[k]) is not int or checkpoint[k] < 0 for k in ('trigger_index', 'action_index')) or
                not isinstance(checkpoint['kind'], str) or not re.fullmatch(r'[A-Za-z]+', checkpoint['kind'])):
            raise ValueError('invalid route checkpoint')
        site = (checkpoint['trigger_index'], checkpoint['action_index'])
        if site in sites:
            raise ValueError('duplicate route checkpoint source site')
        names.add(checkpoint['id'])
        sites.add(site)


def route_orders(row):
    return ''.join(' '.join(map(str, [s['tick'], s['action'], s['side'], s['x'], s['y'], *s['ids']])) + '\n'
                   for s in row['steps'])


def route_identity(row, args):
    """Bind imported evidence to the plan, orders, policy and simulation budget."""
    plan = {k: v for k, v in row.items() if k != 'orders'}
    plan.update(max_ticks=args.max_ticks, no_auto_orders=args.no_auto_orders,
                order_interval=args.order_interval)
    if 'orders' in row:
        plan['orders_text'] = Path(row['orders']).read_text()
    return hashlib.sha256(json.dumps(plan, sort_keys=True, separators=(',', ':')).encode()).hexdigest()


def checkpoint_timeline(row, run):
    if run.get('returncode') != 0 or 'engine' not in run:
        return dict(passed=False, events=[], error='no completed engine report')
    observations = run['engine']['action_observations']
    events, missing = [], []
    for checkpoint in row.get('checkpoints', []):
        site = next((a for a in observations if all(a[k] == checkpoint[k]
                    for k in ('trigger_index', 'action_index', 'kind'))), None)
        if site is None or not site['supported'] or not site['executions']:
            missing.append(checkpoint['id'])
            continue
        events.append(dict(id=checkpoint['id'], tick=site['first_tick']))
    # Aggregated first-dispatch histories cannot establish intra-tick ordering.
    ordered = all(a['tick'] < b['tick'] for a, b in zip(events, events[1:]))
    error = (f'unreached or unsupported checkpoints: {missing}' if missing else
             None if ordered else 'checkpoint order is reversed or ambiguous within one tick')
    return dict(passed=not missing and ordered, events=sorted(events, key=lambda e: e['tick']), error=error)


def reference_comparison(row, directory):
    """Compare two audited reference timelines with the clone, never raw state hashes."""
    pending = dict(equal=None, status='needs live run', first_divergence=None)
    if directory is None:
        return dict(pending, error='no reference timeline directory supplied')
    references = []
    try:
        for repetition in (1, 2):
            path = directory / f"{row['id']}-{repetition}.json"
            value = json.loads(path.read_text())
            if (not isinstance(value, dict) or set(value) !=
                    {'schema', 'contract', 'mission', 'route_sha256', 'status', 'ticks', 'events'} or
                    type(value['schema']) is not int or value['schema'] != 1 or
                    value['contract'] != 'campaign-checkpoints-v1' or
                    type(value['mission']) is not int or value['mission'] != row['mission'] or
                    value['route_sha256'] != row['route_sha256'] or
                    value['status'] not in ('victory', 'defeat', 'timeout') or
                    type(value['ticks']) is not int or not 0 < value['ticks'] <= row['max_ticks'] or
                    not isinstance(value['events'], list)):
                raise ValueError('invalid reference timeline identity or outcome')
            names, previous = set(), 0
            allowed = {c['id'] for c in row.get('checkpoints', [])}
            for event in value['events']:
                if (not isinstance(event, dict) or set(event) != {'id', 'tick'} or
                        not isinstance(event['id'], str) or event['id'] not in allowed or event['id'] in names or
                        type(event['tick']) is not int or not previous < event['tick'] <= value['ticks']):
                    raise ValueError('invalid reference checkpoint sequence')
                names.add(event['id'])
                previous = event['tick']
            references.append(value)
    except (OSError, ValueError) as error:
        return dict(pending, error=str(error))
    if references[0] != references[1]:
        return dict(pending, error='reference timelines disagree between repetitions')
    if (len(row.get('runs', [])) != 2 or not row.get('checkpoint_checks') or
            not all(r.get('returncode') == 0 and r.get('engine', {}).get('status') in
                    ('victory', 'defeat', 'timeout') for r in row['runs'])):
        return dict(pending, error='clone completed outcome is unavailable')
    reference = references[0]
    differences = []
    for run, check in zip(row['runs'], row['checkpoint_checks'], strict=True):
        clone = run['engine']
        for index in range(max(len(reference['events']), len(check['events']))):
            a = reference['events'][index] if index < len(reference['events']) else None
            b = check['events'][index] if index < len(check['events']) else None
            if a != b:
                differences.append(dict(tick=min(e['tick'] for e in (a, b) if e),
                                        field='checkpoint', reference=a, engine=b))
        if (reference['status'], reference['ticks']) != (clone['status'], clone['ticks']):
            differences.append(dict(tick=min(reference['ticks'], clone['ticks']), field='outcome',
                                    reference=dict(status=reference['status'], tick=reference['ticks']),
                                    engine=dict(status=clone['status'], tick=clone['ticks'])))
    first = min(differences, key=lambda d: d['tick']) if differences else None
    return dict(equal=not differences, status='different' if differences else 'equal observed checkpoints and outcome',
                first_divergence=first, scope='first checkpoint dispatch ticks and terminal outcome only')


def mission_matrix(available, selected, rows):
    return [dict(mission=m, availability='present' if m in available else 'missing',
                 selected=m in selected, scenarios=[r for r in rows if r['mission'] == m],
                 interactive_acceptance='pending', progression='pending', defeat_retry='pending') for m in CAMPAIGN]


def write_matrix(output, matrix):
    lines = ['# Campaign coverage matrix', '',
             'Engine consistency and optional reference checkpoint evidence. Interactive controls, defeat/retry and final progression remain pending.', '',
             '| Mission | Data | Scenario / route | Expected / observed | Paired ticks | Mid-mission save | Objective sequence | Reference / first divergence | Diagnostics |',
             '| --- | --- | --- | --- | --- | --- | --- | --- | --- |']
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
                      [c['passed'] for c in row.get('checkpoint_checks', [])],
                      json.dumps(row.get('reference', {'status': 'needs live run', 'first_divergence': None})),
                      json.dumps([d for d in diagnostics if d])]
            lines.append('| ' + ' | '.join(map(cell, values)) + ' |')
    (output / 'matrix.md').write_text('\n'.join(lines) + '\n')


def write_action_findings(output, rows):
    lines = ['# Campaign action findings', '',
             'Engine observations only. Zero executions means not observed in this bounded route; '
             'it does not establish that the original never fires the action. '
             'Enablement and false condition lines describe termination, not historical causes.', '',
             '| Scenario / run | Trigger index / ID | Action index / line | Kind | Support | Executions | First / last tick | Enabled | False condition lines |',
             '| --- | --- | --- | --- | --- | --- | --- | --- | --- |']
    for scenario in rows:
        runs = list(enumerate(scenario.get('runs', []), 1))
        if 'resumed_run' in scenario:
            runs.append(('resume', scenario['resumed_run']))
        for label, run in runs:
            observations = run.get('engine', {}).get('action_observations')
            if observations is None:
                lines.append(f"| {scenario['id']} / {label} | unavailable | | | | | | | |")
                continue
            for a in observations:
                if a['supported'] and a['executions']:
                    continue
                values = [f"{scenario['id']} / {label}", f"{a['trigger_index']} / {a['trigger']}",
                          f"{a['action_index']} / {a['line']}", a['kind'],
                          'implemented' if a['supported'] else 'needs live run', a['executions'],
                          f"{a['first_tick']} / {a['last_tick']}", a['enabled'], a['false_condition_lines']]
                lines.append('| ' + ' | '.join(map(str, values)) + ' |')
    (output / 'actions.md').write_text('\n'.join(lines) + '\n')


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
    parser.add_argument('--scenarios', '--routes', type=Path, help='JSON route list with steps, checkpoints, outcomes and save ticks')
    parser.add_argument('--reference-dir', type=Path, help='private audited ID-1.json and ID-2.json reference timelines')
    parser.add_argument('--require-reference', action='store_true', help='fail unless repeated reference timelines agree with clone')
    parser.add_argument('--no-auto-orders', action='store_true')
    args = parser.parse_args(argv)
    args.game = args.game.resolve()
    args.engine = args.engine.resolve()
    args.rules_dir = args.rules_dir.resolve()
    if args.reference_dir:
        args.reference_dir = args.reference_dir.resolve()
    if args.orders_dir:
        args.orders_dir = args.orders_dir.resolve()
    try:
        maps, available = discover(args.game)
        missions = sorted(set(args.missions or CAMPAIGN))
        if set(missions) - set(CAMPAIGN) or args.max_ticks < 2:
            raise ValueError('campaign missions must be 1 through 15; max-ticks must be at least 2')
        planned = scenarios(args.scenarios.resolve() if args.scenarios else None, missions, args.max_ticks)
        if any('steps' in s for s in planned) and (not args.no_auto_orders or args.orders_dir):
            raise ValueError('inline route steps require --no-auto-orders and cannot use --orders-dir')
        for row in planned:
            if args.orders_dir and 'orders' not in row:
                fallback = args.orders_dir / f"mission_{row['mission']}.orders"
                if fallback.is_file():
                    row['orders'] = str(fallback)
            row['route_sha256'] = route_identity(row, args)
        if not args.engine.is_file():
            raise ValueError(f'engine does not exist: {args.engine}')
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        output = (args.out or ROOT / 'work' / f'playthrough-{stamp}').resolve()
        private_root = (ROOT / 'work').resolve()
        if output == private_root or not output.is_relative_to(private_root):
            raise ValueError('--out must be a new directory under this worktree work/')
        if output.is_relative_to(args.game) or args.game.is_relative_to(output):
            raise ValueError('output and source game must not overlap')
        sources = [args.rules_dir, args.orders_dir, args.reference_dir, args.scenarios]
        sources.extend(Path(s['orders']) for s in planned if 'orders' in s)
        for source in sources:
            source = source.resolve() if source else None
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
        if args.reference_dir:
            reference_source = args.reference_dir
            args.reference_dir = output / 'reference'
            args.reference_dir.mkdir()
            for row in planned:
                for repetition in (1, 2):
                    name = f"{row['id']}-{repetition}.json"
                    if (reference_source / name).is_file():
                        shutil.copyfile(reference_source / name, args.reference_dir / name)
        # Snapshot scenario orders alongside the plan; source files remain private.
        for row in planned:
            if 'steps' in row:
                target = output / (row['id'] + '.orders')
                target.write_text(route_orders(row))
                row['orders'] = str(target)
            elif 'orders' in row:
                target = output / (row['id'] + '.orders')
                shutil.copyfile(row['orders'], target)
                row['orders'] = str(target)
        (output / 'scenarios.json').write_text(json.dumps(planned, indent=2) + '\n')
        # Separate installations for both processes, including copied profile/configuration files.
        games = [output / f'game-{i}' for i in (1, 2)]
        for game in games:
            private_game(args.game, maps, game)
        summary = {'schema': 2, 'method': 'engine consistency with optional audited reference checkpoints; no full-state parity claim',
                   'engine_sha256': hashlib.sha256(args.engine.read_bytes()).hexdigest(),
                   'max_ticks': args.max_ticks, 'wall_timeout': args.timeout, 'missions': [],
                   'acceptance': 'pending interactive controls, defeat/retry and final progression'}
        failed = bool(set(missions) - available.keys())
        failed |= any(not any(s['mission'] == m for s in planned) for m in missions)
        for scenario in planned:
            mission = scenario['mission']
            row = dict(scenario, runs=[], max_ticks=args.max_ticks)
            row['reference'] = dict(equal=None, status='needs live run', first_divergence=None)
            summary['missions'].append(row)
            if mission in available:
                orders = Path(scenario['orders']) if 'orders' in scenario else None
                runs = [run_once(args, mission, game, output / f"{scenario['id']}-run-{i}",
                                 orders=orders, save_tick=scenario['save_tick'])
                        for i, game in enumerate(games, 1)]
                row['runs'] = runs
                row['checkpoint_checks'] = [checkpoint_timeline(scenario, r) for r in runs]
                row['paired_ticks'] = paired_hashes(*runs)
                terminal_match, tick_match = compare(runs), row['paired_ticks']['equal']
                row['deterministic'] = (None if terminal_match is None or tick_match is None
                                        else terminal_match and tick_match)
                saved = output / f"{scenario['id']}-run-1" / 'midmission.azs'
                if saved.is_file() and runs[0].get('engine', {}).get('ticks', 0) > scenario['save_tick']:
                    resumed = run_once(args, mission, games[1], output / f"{scenario['id']}-resume",
                                       orders=orders, resume=saved)
                    row['resumed_run'] = resumed
                    row['resume_checkpoints'] = checkpoint_timeline(scenario, resumed)
                    row['save_continuation'] = paired_hashes(runs[0], resumed, scenario['save_tick'])
                    if row['save_continuation']['equal'] is True:
                        row['save_continuation']['equal'] = all(
                            runs[0]['engine'][k] == resumed['engine'][k] for k in ('ticks', 'status', 'state_hash'))
                else:
                    row['save_continuation'] = {'equal': None, 'error': 'no completed baseline with a mid-mission save'}
                row['passed'] = (row['deterministic'] is True and row['save_continuation']['equal'] is True and
                                 row.get('resume_checkpoints', {}).get('passed') is True and
                                 all(c['passed'] for c in row['checkpoint_checks']) and
                                 all(r['status'] == scenario['expected_outcome'] and
                                     not any(r.get('engine', {}).get(k) for k in
                                             ('script_errors', 'unreachable_objectives', 'unsupported_executions'))
                                     for r in runs))
            else:
                row.update(passed=False, error='missing mission data')
            row['reference'] = reference_comparison(row, args.reference_dir)
            row['verified'] = row['passed'] and row['reference']['equal'] is True
            if args.reference_dir or args.require_reference:
                row['passed'] &= row['reference']['equal'] is True
            print(f"{scenario['id']}: {' / '.join(r['status'] for r in row['runs']) or 'not run'}; "
                  f"passed={row['passed']}; reference={row['reference']['status']}", flush=True)
            # Only complete paired observations qualify for aggregate parity counts.
            if row['reference']['equal'] is not None:
                diff_dir = output / (row['id'] + '-reference')
                diff_dir.mkdir()
                category = 'equal_observed_events' if row['reference']['equal'] else 'timing'
                first = row['reference']['first_divergence']
                if first and first['field'] == 'outcome':
                    category = 'field_difference'
                elif first and (first['reference'] is None or first['engine'] is None or
                                first['reference']['id'] != first['engine']['id']):
                    category = 'identity_or_order'
                (diff_dir / 'diff.json').write_text(json.dumps(dict(
                    layer='L7', category=category, **row['reference']), indent=2) + '\n')
            failed |= not row['passed']
            summary['matrix'] = mission_matrix(available, missions, summary['missions'])
            (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
            write_matrix(output, summary['matrix'])
            write_action_findings(output, summary['missions'])
        # Emit a complete matrix even when the selected plan contains no scenarios.
        summary['matrix'] = mission_matrix(available, missions, summary['missions'])
        (output / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
        write_matrix(output, summary['matrix'])
        write_action_findings(output, summary['missions'])
        print(f'Report: {output / "summary.json"}')
        return 1 if failed else 0
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    sys.exit(main())
