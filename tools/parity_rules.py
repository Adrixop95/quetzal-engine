#!/usr/bin/env python3
"""L1 candidate launch controls and private comparisons, pending audited adapters."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
_spec = importlib.util.spec_from_file_location('parity_report', ROOT / 'tools/parity_report.py')
report = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(report)


def scenarios():
    cases = []
    for path in sorted((ROOT / 'tests/parity').glob('l1_*.json')):
        case = report.json_object(path.read_text())
        required = {'id', 'layer', 'map', 'setup', 'input', 'observe', 'tolerance', 'evidence', 'mechanic'}
        if (set(case) != required or case['id'] != path.stem or case['layer'] != 'L1'
                or case['evidence'] != 'P' or case['tolerance'] != 0):
            raise ValueError(f'invalid provisional L1 specification: {path}')
        if not re.fullmatch(r'l1_[a-z0-9_]+', case['id']):
            raise ValueError('unsafe scenario identifier')
        objects = case['setup']['preplaced_objects']
        ids = [o['id'] for o in objects]
        if not ids or len(ids) != len(set(ids)):
            raise ValueError('scenario requires unique named objects')
        for obj in objects:
            if any(type(obj[k]) is not int for k in ('type', 'side', 'x', 'y')):
                raise ValueError('object coordinates, type and side must be integers')
            if not (0 <= obj['side'] <= 1 and 2 <= obj['x'] < 88 and 2 <= obj['y'] < 58):
                raise ValueError('object outside candidate map interior')
        observation = case['observe']
        if (observation['first_tick'] != 1 or type(observation['last_tick']) is not int
                or observation['last_tick'] < 1 or not observation['fields']
                or len(set(observation['fields'])) != len(observation['fields'])):
            raise ValueError('invalid observation horizon/fields')
        cases.append(case)
    if not cases:
        raise ValueError('no L1 specifications found')
    return cases


def measurement_protocol(cases):
    """Validate the pending measurement recipe without manufacturing observations."""
    protocol = report.json_object((ROOT / 'tests/parity/rules_protocol.json').read_text())
    rows = protocol['cases']
    if (protocol['schema'] != 1 or protocol['status'] != 'needs live run'
            or protocol['phase'] != 'before_commands'
            or [row['id'] for row in rows] != [case['id'] for case in cases]
            or [row['order'] for row in rows] != list(range(1, len(cases) + 1))
            or set(protocol['fields']) != {field for case in cases for field in case['observe']['fields']}):
        raise ValueError('measurement protocol must cover all scenarios and fields in order')
    for row in rows:
        if (row['expected'] is not None or row['evidence_label'] is not None
                or any(not isinstance(row[key], str) or not row[key]
                       for key in ('metric', 'unit', 'definition'))):
            raise ValueError('pending protocol cannot claim measured expectations or evidence')
    return protocol


def plan(cases, output):
    """Write an asset-free handoff; never launch processes or apply scenario inputs."""
    protocol = measurement_protocol(scenarios())
    selected = {case['id'] for case in cases}
    output = report.private_output(output)
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    for recipe in protocol['cases']:
        if recipe['id'] not in selected:
            continue
        case = next(case for case in cases if case['id'] == recipe['id'])
        directory = output / case['id']
        directory.mkdir()
        report.write_json(directory / 'scenario.json', case)
        row = dict(recipe, reference=None, clone_before=None, clone_after=None,
                   status='needs live run', evidence='P',
                   controls=case['setup']['controls'])
        report.write_json(directory / 'measurement.json', row)
        rows.append(row)
    report.write_json(output / 'normalization.json', protocol)
    report.write_json(output / 'measurements.json', rows)
    return 0


def seed_game(source, destination):
    """Copy only the map template and writable settings; immutable assets are links."""
    destination.mkdir()
    (destination / 'Maps').mkdir()
    (destination / 'Saves').mkdir()
    for name in ('Aztec.exe', 'Res', 'Video', 'Music', 'Campaign'):
        if (source / name).exists():
            (destination / name).symlink_to((source / name).resolve())
    for item in source.iterdir():
        if item.is_file() and item.suffix.lower() in ('.ini', '.plr'):
            shutil.copy2(item, destination / item.name)
    for item in (source / 'Maps').glob('mission_1.*'):
        shutil.copy2(item, destination / 'Maps' / item.name)


def prepare(case, game, directory):
    seed_game(game, directory / 'game')
    private = directory / 'game'
    env = {k: v for k, v in os.environ.items() if not k.startswith(('RT_', 'ZSTAT_'))}
    env['ZSTAT_GAME_DIR'] = str(private)
    specs = [f"{o['side']}:{o['type']}:1:{o['x']}:{o['y']}" for o in case['setup']['preplaced_objects']]
    # No provisional high-level command is silently translated into an oracle packet.
    with (directory / 'generation.log').open('w') as log:
        subprocess.run([sys.executable, '-I', str(ROOT / 'tools/make_zstat.py'), case['id'], *specs],
                       cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, check=True)
    maps = private / 'Maps'
    width, height = struct.unpack('<HH', (maps / (case['id'] + '.map')).read_bytes()[:4])
    if (width, height) != (90, 60):
        raise ValueError('candidate generator requires the mission-1 90 by 60 template')
    (maps / (case['id'] + '.dat')).write_bytes(bytes(
        2 if 2 <= x < width - 2 and 2 <= y < height - 2 else 0
        for y in range(height) for x in range(width)))
    (maps / (case['id'] + '.rm')).write_bytes(struct.pack('<HH', width, height) + bytes(2 * width * height))
    report.write_json(directory / 'scenario.json', case)
    report.write_json(directory / 'pending.json', dict(
        category='blocked_setup_contract',
        detail='Candidate placements only: command translation, AI/fog/HP/tech controls, town membership and observers require task 64.',
        unimplemented_input=case['input'], controls=case['setup']['controls']))
    return private


def clone_probe(case, engine, directory, timeout):
    """Exercise the candidate independently; engine-local fields are not L1 evidence."""
    if not (directory / 'game').is_dir():
        raise ValueError('clone candidate was not prepared')
    observations = directory / 'observations.jsonl'
    report.execute([engine, '--headless', '--game', directory / 'game',
                    '--rules-dir', ROOT / 'docs/data', '--no-auto-orders',
                    '--mod', 'parallel_construction=off', '--max-ticks', case['observe']['last_tick'],
                    '--report', directory / 'report.json', '--observations', observations, case['id']],
                   directory, timeout, dict(AZTEC_GAME=str(directory / 'game')), setup_network=False)
    summary = report.json_object((directory / 'report.json').read_text())
    if (summary['ticks'] != case['observe']['last_tick'] or summary['script_errors']
            or summary['unsupported_executions']):
        raise ValueError('clone candidate did not complete the supported observation horizon')
    with observations.open() as stream:
        meta = report.header(stream)
    expected = dict(schema=1, mission=0, phase='before_commands', contract='engine-index-v1',
                    fields=['hp', 'side', 'type', 'x', 'y'], first_tick=1,
                    last_tick=case['observe']['last_tick'])
    if meta != expected:
        raise ValueError('unexpected clone candidate observation contract')
    report.compare(observations, observations)
    return report.result('blocked_setup_contract',
                         detail='Clone candidate completed; L1 setup, commands and normalized fields remain unaudited.')


def probe(cases, game, runtime, output, timeout, engine=None):
    output = report.private_output(output)
    required = [runtime, game / 'Aztec.exe', game / 'Res', *[
        game / f'Maps/mission_1.{ext}' for ext in ('mp', 'map', 'dat', 'rm', 'vis', 'spr', 'mit')]]
    if engine is not None:
        required.append(engine)
    missing = [str(p) for p in required if not p.exists()]
    if missing:
        print('SKIP: missing private inputs: ' + ', '.join(missing))
        return 77
    output.mkdir(parents=True, exist_ok=False)
    rows = []
    for case in cases:
        row = dict(id=case['id'], layer='L1', tick=None, object_id=None, field=None)
        directory = output / case['id']
        directory.mkdir()
        try:
            private = prepare(case, game, directory)
            # This is a launch/control probe, not an implementation of the scenario inputs.
            script = '\n'.join(('wait 6000', 'click 140 239', 'wait 800', 'click 160 543',
                                'wait 5000', 'click 650 58', 'wait 2500', 'click 150 258',
                                'wait 800', 'click 160 543', 'wait 12000', 'quit', ''))
            (directory / 'start.txt').write_text(script)
            # Only the requested candidate may appear in the custom-map list.
            for path in (private / 'Maps').glob('mission_1.*'):
                path.unlink()
            if engine is not None:
                # Snapshot before the reference can change settings, saves or maps.
                (directory / 'clone').mkdir()
                report.private_game(private, directory / 'clone/game')
            report.execute([runtime, private / 'Aztec.exe'], directory, timeout, dict(
                RT_SCRIPT=str(directory / 'start.txt'), RT_NO_GRAB='1', RT_NO_VIDEO='1',
                RT_RESOLUTION='800x600', RT_STATE_LOG=str(directory / 'state.txt'),
                RT_CMD_LOG=str(directory / 'commands.txt'), RT_UNIT_LOG=str(directory / 'units.txt'),
                RT_UNIT_EVERY='1', RT_DUMP_DIR=str(directory)))
            row.update(category='blocked_setup_contract', detail='Launch returned; scenario commands and observations remain unaudited.')
        except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as exc:
            row.update(category='experiment_failed', detail=str(exc))
        outcomes = {'reference': report.result(row['category'], detail=row['detail'])}
        if engine is not None:
            try:
                # A failed reference launch must not suppress independent clone diagnostics.
                outcomes['clone'] = clone_probe(case, engine, directory / 'clone', timeout)
            except (OSError, ValueError, RuntimeError, StopIteration) as exc:
                outcomes['clone'] = report.result('experiment_failed', detail=str(exc))
            if outcomes['clone']['category'] == 'experiment_failed' and row['category'] != 'experiment_failed':
                row.update(outcomes['clone'])
        report.write_json(directory / 'runners.json', outcomes)
        report.write_json(directory / 'diff.json', row)
        rows.append(row)
        print(f"{case['id']}: {row['category']}")
    report.write_json(output / 'report.json', rows)
    return 1


def evaluate(case, reference, repeat, clone, output):
    """Export only repeated, complete numeric observations; retain first clone divergence."""
    output = report.private_output(output)
    output.mkdir(parents=True, exist_ok=False)
    for path in (reference, repeat, clone):
        with path.open() as stream:
            meta = report.header(stream)
        if (meta['contract'] != f"l1:{case['id']}:v1" or meta['first_tick'] != case['observe']['first_tick']
                or meta['last_tick'] != case['observe']['last_tick']
                or set(meta['fields']) != set(case['observe']['fields'])):
            raise ValueError('adapter contract, horizon or observed fields do not cover the scenario')
    repeated = report.compare(reference, repeat)
    if repeated['category'] != 'equal_observed_fields':
        verdict = dict(repeated, category='reference_semantic_nondeterminism')
    else:
        verdict = report.compare(reference, clone)
        with reference.open() as stream:
            meta = report.header(stream)
            snapshots = list(report.snapshots(stream, meta))
        identities = sorted({identity for row in snapshots for identity in row['objects']})
        required = {obj['id'] for obj in case['setup']['preplaced_objects']}
        if not required.issubset(snapshots[0]['objects']):
            raise ValueError('initial observations must identify every scenario object')
        fields = sorted(meta['fields'])
        indices = {identity: index for index, identity in enumerate(identities)}
        numbers = [[row['tick'], indices[identity], index, values[field]]
                   for row in snapshots for identity, values in sorted(row['objects'].items())
                   for index, field in enumerate(fields)]
        report.write_json(output / 'expectations.json', numbers)
        report.write_json(output / 'expectation_keys.json', dict(
            scenario=case['id'], header=meta, objects=identities, fields=fields,
            columns=['tick', 'object_index', 'field_index', 'value']))
    report.write_json(output / 'diff.json', dict(verdict, layer='L1'))
    return 0 if verdict['category'] == 'equal_observed_fields' else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='mode', required=True)
    sub.add_parser('list')
    planning = sub.add_parser('plan', help='write pending measurement recipes without assets or live execution')
    planning.add_argument('--output', type=Path, required=True)
    planning.add_argument('--id')
    run = sub.add_parser('probe', help='generate candidates and attempt isolated launch controls')
    run.add_argument('--game', type=lambda p: Path(p).resolve(), required=True)
    run.add_argument('--runtime', type=lambda p: Path(p).resolve(), required=True)
    run.add_argument('--engine', type=lambda p: Path(p).resolve(), help='also run an independent clone candidate control')
    run.add_argument('--output', type=Path, required=True)
    run.add_argument('--id')
    run.add_argument('--timeout', type=int, default=45)
    compare = sub.add_parser('compare', help='consume audited task-42-style normalized streams')
    compare.add_argument('--id', required=True)
    for name in ('reference', 'repeat', 'clone', 'output'):
        compare.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    try:
        cases = scenarios()
        if args.mode == 'list':
            for case in cases:
                print(f"{case['id']} {case['mechanic']} P")
            return 0
        if args.id:
            cases = [case for case in cases if case['id'] == args.id]
            if not cases:
                raise ValueError('unknown L1 scenario')
        if args.mode == 'compare':
            return evaluate(cases[0], args.reference, args.repeat, args.clone, args.output)
        if args.mode == 'plan':
            return plan(cases, args.output)
        if args.timeout <= 0:
            raise ValueError('timeout must be positive')
        return probe(cases, args.game, args.runtime, args.output, args.timeout, args.engine)
    except (OSError, ValueError, RuntimeError, StopIteration) as exc:
        parser.exit(2, f'parity_rules: {exc}\n')


if __name__ == '__main__':
    raise SystemExit(main())
