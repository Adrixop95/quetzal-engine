#!/usr/bin/env python3
"""Private UI captures: screen catalogue (default) and front-end scenario (`frontend` subcommand).

Both report repeatability separately from parity; neither claims L4/L5 parity on its own.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import sys
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parent))
from parity_report import ROOT, execute, private_game, private_output, write_json
from parity_visual import image_file, load, require

CATALOGUE = ROOT / 'tests/parity/ui/screens.json'
KEYS = {'Escape': 1, 'Return': 28, 'Space': 57, 'F1': 59}


def validate(spec):
    require(spec.get('schema') == 1, 'unsupported UI schema')
    rows = spec.get('screens')
    require(isinstance(rows, list) and rows, 'screen list required')
    ids = set()
    for row in rows:
        name = row.get('id', '')
        require(isinstance(name, str) and re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*', name)
                and name not in ids, 'invalid or duplicate screen ID')
        ids.add(name)
        require(isinstance(row.get('reach'), str) and bool(row['reach']), 'reach description required')
        require(row.get('evidence') == 'P', 'routes remain proposals until independently observed')
        require(set(row.get('runners', {})) == {'reference', 'clone'}, 'two runner routes required')
        for route in row['runners'].values():
            require(isinstance(route, dict), 'invalid route')
            if 'blocked' in route:
                require(set(route) == {'blocked'} and isinstance(route['blocked'], str)
                        and bool(route['blocked']), 'invalid blocker')
                continue
            require(set(route) == {'mission', 'input'}, 'invalid route fields')
            require(route['mission'] is None or type(route['mission']) is int
                    and 1 <= route['mission'] <= 5, 'invalid mission')
            events = route['input']
            require(isinstance(events, list) and len(events) <= 4096, 'invalid input list')
            total = 0
            for event in events:
                require(isinstance(event, list) and bool(event), 'invalid event')
                cmd, *args = event
                if cmd == 'wait':
                    require(len(args) == 1 and type(args[0]) is int and 1 <= args[0] <= 10000,
                            'wait is a positive number of frames')
                    total += args[0]
                elif cmd == 'move':
                    require(len(args) == 2 and all(type(x) is int for x in args)
                            and 0 <= args[0] < 800 and 0 <= args[1] < 600, 'invalid logical position')
                    total += 1
                elif cmd in ('down', 'up'):
                    require(not args, 'button event takes no arguments')
                    total += 1
                elif cmd == 'key':
                    require(len(args) == 1 and args[0] in KEYS, 'unsupported shared key')
                    total += 2
                else:
                    raise ValueError('unsupported event (wall-clock waits are forbidden)')
            require(total <= 20000, 'frame horizon too large')
    return rows


def scripts(route, shot):
    """One event per frame; key release gets its own frame on both runners."""
    clone, reference, frame = [], [], 0
    for cmd, *args in route['input']:
        if cmd == 'wait':
            clone.append(f'wait {args[0]}')
            frame += args[0]
        elif cmd == 'key':
            clone.extend([f'key {args[0]}', 'wait 1'])
            reference.extend([f'{frame} keydown {KEYS[args[0]]}', f'{frame + 1} keyup {KEYS[args[0]]}'])
            frame += 2
        else:
            text = ' '.join(map(str, [cmd, *args]))
            clone.append(text)
            reference.append(f'{frame} {text}')
            frame += 1
    clone.extend([f'shot {shot}', 'quit'])
    reference.extend([f'{frame} shot', f'{frame + 1} quit'])
    return '\n'.join(reference) + '\n', '\n'.join(clone) + '\n'


def png_bytes(w, h, pixels):
    """Canonical opaque RGB PNG: no timestamps or tool-specific metadata."""
    require(len(pixels) == w * h * 3, 'RGB raster required')
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    raw = b''.join(b'\0' + pixels[y * w * 3:(y + 1) * w * 3] for y in range(h))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def metrics(left, right):
    require(left[:3] == right[:3], 'capture dimensions differ')
    delta = [abs(a - b) for a, b in zip(left[3], right[3], strict=True)]
    n = len(delta)
    different = sum(any(delta[i:i + 3]) for i in range(0, n, 3))
    return dict(pixels=n // 3, different_pixels=different, mismatch_fraction=different / (n // 3),
                mae=sum(delta) / n, rmse=math.sqrt(sum(d * d for d in delta) / n),
                maximum_channel_error=max(delta))


def capture(args, row, runner, directory):
    route = row['runners'][runner]
    if 'blocked' in route:
        return dict(status='blocked_route', reason=route['blocked'])
    binary = args.runtime if runner == 'reference' else args.clone
    if not args.game.is_dir() or not binary.is_file():
        return dict(status='missing_private_input')
    captures, encoded, originals = [], [], []
    errors = []
    for repeat in range(args.repeats):
        run = directory / runner / str(repeat)
        run.mkdir(parents=True)
        try:
            game = run / 'game'
            private_game(args.game, game)
            script = run / 'input.txt'
            ref, clone = scripts(route, Path('frame.png'))
            script.write_text(ref if runner == 'reference' else clone)
            if runner == 'reference':
                argv = [binary.resolve(), game / 'Aztec.exe']
                env = dict(RT_UI_SCRIPT=str(script), RT_NO_GRAB='1', RT_NO_VIDEO='1',
                           RT_RESOLUTION='800x600', RT_DUMP_DIR=str(run))
            else:
                argv = [binary.resolve(), '--game', game, '--rules-dir', ROOT / 'docs/data',
                        '--size', '800x600', '--software', '--no-audio', '--no-mouse-grab']
                env = {}
                if route['mission'] is None:
                    env['AZTEC_UI_SCRIPT'] = str(script)
                else:
                    argv += [str(route['mission']), '--script', script, '--no-edge-scroll', '--menu-session',
                             '--ui-trace', run / 'ui.jsonl']
            execute(argv, run, args.timeout, env, setup_network=runner == 'reference')
            original = run / ('shot_01.ppm' if runner == 'reference' else 'frame.png')
            if runner == 'reference':
                require('UI capture contract: guest-present-v1' in (run / 'process.log').read_text(),
                        'reference binary lacks RT_UI_SCRIPT; rebuild the private runtime')
            raster = image_file(original)
            require(raster[:3] == (800, 600, 3), 'expected opaque 800x600 RGB capture')
            captures.append(raster)
            encoded.append(png_bytes(800, 600, raster[3]))
            originals.append(original.read_bytes())
        except (ValueError, RuntimeError, OSError) as exc:
            errors.append(dict(repeat=repeat, reason=str(exc)))
    if errors:
        return dict(status='capture_failed', errors=errors)
    # Preserve diagnostic frames even on nondeterminism; status must be consulted.
    (directory / f'{runner}.png').write_bytes(encoded[0])
    deterministic = all(data == originals[0] for data in originals)
    return dict(status='repeatable_capture' if deterministic else 'nondeterministic_capture',
                raw_bytes_equal=deterministic, png_bytes_equal=all(data == encoded[0] for data in encoded),
                sha256=[hashlib.sha256(data).hexdigest() for data in originals],
                repeat_metrics=[metrics(captures[0], other) for other in captures[1:]])


def run(args):
    rows = validate(load(args.spec))
    if args.screen:
        require(set(args.screen) <= {row['id'] for row in rows}, 'unknown screen ID')
        rows = [row for row in rows if row['id'] in args.screen]
    require(args.all or args.screen, 'select --screen ID or --all')
    root = private_output(args.output)
    root.mkdir(parents=True, exist_ok=True)
    reports = []
    for row in rows:
        directory = root / row['id']
        directory.mkdir(exist_ok=True)
        phase = directory / args.phase
        require(not phase.exists(), 'phase already exists; use a fresh output root or another phase')
        phase.mkdir()
        write_json(phase / 'scenario.json', row)
        for runner in ('reference', 'clone'):
            (directory / f'{runner}.png').unlink(missing_ok=True)
        report = dict(id=row['id'], phase=args.phase, runners={}, difference=None,
                      status='unmeasured', l4_l5='unverified_alignment_and_semantic_state')
        for runner in ('reference', 'clone'):
            report['runners'][runner] = capture(args, row, runner, phase)
            frame = phase / f'{runner}.png'
            if frame.exists():
                shutil.copyfile(frame, directory / frame.name)
        if all((phase / f'{runner}.png').exists() for runner in ('reference', 'clone')):
            report['difference'] = metrics(image_file(phase / 'reference.png'), image_file(phase / 'clone.png'))
            report['status'] = 'diagnostic_comparison'
        write_json(phase / 'report.json', report)
        write_json(directory / f'{args.phase}.json', report)
        reports.append(report)
        print(row['id'] + ': ' + ', '.join(f'{k}={v["status"]}' for k, v in report['runners'].items()), flush=True)
    write_json(root / f'{args.phase}.json', reports)
    comparisons = []
    for row in rows:
        phases = {}
        for name in ('before', 'after'):
            path = root / row['id'] / f'{name}.json'
            phases[name] = load(path)['difference'] if path.exists() else None
        comparisons.append(dict(id=row['id'], **phases))
    write_json(root / 'before-after.json', comparisons)
    # Captures alone cannot establish L4 alignment or L5 command equivalence.
    return 1 if any(row['status'] == 'unmeasured'
                    or any(r['status'] != 'repeatable_capture' for r in row['runners'].values())
                    or row['difference']['different_pixels'] for row in reports) else 0


def load_spec(path):
    spec = json.loads(path.read_text())
    require(spec.get('schema') == 1 and spec.get('evidence') == 'P', 'expected provisional UI scenario')
    require(spec.get('resolution') == [800, 600], 'only the documented 800x600 route is supported')
    require(isinstance(spec.get('clone'), list) and spec['clone'], 'clone route required')
    previous, observations = -1, set()
    for event in spec['clone']:
        require(type(event.get('frame')) is int and previous <= event['frame'] <= 100000
                and event['frame'] >= 0, 'invalid frame schedule')
        previous = event['frame']
        command, args = event.get('command'), event.get('args', [])
        require(isinstance(args, list), 'invalid event arguments')
        if command == 'move':
            require(len(args) == 2 and all(type(v) is int for v in args), 'move requires coordinates')
        elif command in ('keydown', 'keyup'):
            require(len(args) == 1 and isinstance(args[0], str) and args[0]
                    and not any(c in args[0] for c in '\n\r#'), 'invalid key')
        elif command == 'shot':
            require(len(args) == 1 and isinstance(args[0], str)
                    and args[0] and all(c in 'abcdefghijklmnopqrstuvwxyz0123456789_-' for c in args[0])
                    and args[0] not in observations, 'invalid/duplicate screen name')
            observations.add(args[0])
        else:
            require(command in ('down', 'up', 'close', 'quit') and not args, 'unsupported input')
    require(observations, 'capture points required')
    reference = spec.get('reference', {})
    require(isinstance(reference.get('input'), list) and reference['input'], 'reference probe required')
    require(isinstance(reference.get('screens'), list) and reference['screens']
            and all(isinstance(s, str) and s for s in reference['screens']), 'reference screen names required')
    require(all(isinstance(line, str) and '\n' not in line and '\r' not in line
                and line.split() and line.split()[0] in ('wait', 'move', 'click', 'down', 'up', 'key', 'shot', 'quit')
                for line in reference['input']), 'unsupported reference probe')
    require(reference['input'].count('shot') == len(reference['screens']), 'reference capture count differs')
    return spec


def pixels(left, right):
    a, b = image_file(left), image_file(right)
    require(a[:3] == b[:3] and a[2] == 3, 'capture dimensions or channels differ')
    count = a[0] * a[1]
    changed = sum(a[3][i:i + 3] != b[3][i:i + 3] for i in range(0, len(a[3]), 3))
    return dict(pixels=count, changed_pixels=changed,
                mae=sum(abs(x - y) for x, y in zip(a[3], b[3], strict=True)) / (3 * count),
                identical_file=left.read_bytes() == right.read_bytes())


def capture_clone(args, spec, directory):
    directory.mkdir()
    game = directory / 'game'
    private_game(args.game.resolve(), game)
    script, shots = [], {}
    for event in spec['clone']:
        command, values = event['command'], event.get('args', [])
        if command == 'shot':
            name = values[0]
            shots[name] = directory / (name + '.png')
            values = [str(shots[name])]
        script.append(f"{event['frame']} {command}" + ''.join(' ' + str(v) for v in values))
    (directory / 'input.txt').write_text('\n'.join(script) + '\n')
    execute([args.clone.resolve(), '--game', game, '--rules-dir', ROOT / 'docs/data',
             '--size', '800x600', '--software', '--no-audio', '--menu-script', directory / 'input.txt',
             '--menu-trace', directory / 'trace.jsonl'], directory, args.timeout,
            dict(AZTEC_USER_DIR=str(directory / 'profile'), AZTEC_UI_SCRIPT='', AZTEC_MENU_SHOT=''), setup_network=False)
    rows = [json.loads(line) for line in (directory / 'trace.jsonl').read_text().splitlines()]
    require(rows and all(r['contract'] == 'engine-menu-raw-v1' and r['frame'] == i
                         for i, r in enumerate(rows)), 'invalid/truncated menu trace')
    require(len(rows) == spec['clone'][-1]['frame'] + 1, 'route ended before its declared horizon')
    observed = {Path(r['capture']).stem: r for r in rows if r['capture']}
    require(set(observed) == set(shots), 'capture schedule differs')
    for name, shot in shots.items():
        image_file(shot)
        require(observed[name]['rendered_screen'] == spec['screens'][name], f'unexpected screen at {name}')
    # Absolute output paths differ across repetitions; all other fields must agree.
    for row in rows:
        if row['capture']:
            row['capture'] = Path(row['capture']).name
    return shots, rows


def run_frontend(args):
    spec = load_spec(args.scenario)
    output = private_output(args.output)
    require(not output.exists(), 'use a fresh output directory')
    output.mkdir(parents=True)
    results = {'category': 'unmeasured_reference_parity', 'scenario': spec['id'],
               'audio': 'unobserved', 'reference_timing': 'wall_clock_unaligned',
               'clone_repeatability': {}, 'paired_pixels': {}, 'failures': {}}
    reference = output / 'reference'
    reference.mkdir()
    try:
        private_game(args.game.resolve(), reference / 'game')
        require(args.runtime.is_file(), 'reference runtime unavailable')
        script = reference / 'input.txt'
        script.write_text('\n'.join(spec['reference']['input']) + '\n')
        execute([args.runtime.resolve(), reference / 'game/Aztec.exe'], reference, args.timeout,
                dict(RT_SCRIPT=str(script), RT_DUMP_DIR=str(reference), RT_NO_VIDEO='1',
                     RT_NO_GRAB='1', RT_RESOLUTION='800x600', RT_AUDIO_LOG=str(reference / 'audio.jsonl')))
        shots = sorted(reference.glob('shot_*.ppm'))
        require(len(shots) == len(spec['reference']['screens']), 'reference capture count differs')
        for shot in shots:
            image_file(shot)
        results['reference_captures'] = dict(zip(spec['reference']['screens'], map(str, shots), strict=True))
    except (ValueError, RuntimeError, OSError) as exc:
        results['failures']['reference'] = str(exc)
    clones = []
    for repetition in (1, 2):
        try:
            clones.append(capture_clone(args, spec, output / f'clone-{repetition}'))
        except (ValueError, RuntimeError, OSError) as exc:
            results['failures'][f'clone-{repetition}'] = str(exc)
    if len(clones) == 2:
        results['clone_trace_equal'] = clones[0][1] == clones[1][1]
        results['clone_repeatability'] = {name: pixels(path, clones[1][0][name]) for name, path in clones[0][0].items()}
        # Exploratory pixels never satisfy L4/L5: the supplied runtime has no
        # fixed-frame front-end observer or shared semantic state contract.
        results['unaligned_exploratory_pixels'] = {
            name: pixels(Path(path), clones[0][0][name])
            for name, path in results.get('reference_captures', {}).items() if name in clones[0][0]}
    write_json(output / 'report.json', results)
    print(json.dumps(results, indent=2))
    return 1  # An unaligned probe must never report a parity pass.


def screens_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--spec', type=Path, default=CATALOGUE)
    parser.add_argument('--screen', action='append')
    parser.add_argument('--all', action='store_true')
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--phase', choices=('before', 'after'), default='after')
    parser.add_argument('--output', type=Path, default=ROOT / 'work/parity/ui')
    parser.add_argument('--game', type=Path, default=ROOT / 'game')
    parser.add_argument('--runtime', type=Path, default=ROOT / 'build/aztec_recomp')
    parser.add_argument('--clone', type=Path, default=ROOT / 'build/aztec_engine')
    parser.add_argument('--timeout', type=int, default=30)
    parser.add_argument('--repeats', type=int, default=2)
    return parser


def frontend_parser():
    parser = argparse.ArgumentParser(prog='ui_capture.py frontend', description=__doc__)
    parser.add_argument('--scenario', type=Path, default=ROOT / 'tests/parity/ui/frontend.json')
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--clone', type=Path, default=ROOT / 'build/aztec_engine')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=int, default=30)
    return parser


def main():
    frontend = sys.argv[1:2] == ['frontend']
    args = frontend_parser().parse_args(sys.argv[2:]) if frontend else screens_parser().parse_args()
    try:
        if frontend:
            require(1 <= args.timeout <= 300, 'timeout must be 1..300 seconds')
            return run_frontend(args)
        require(2 <= args.repeats <= 10 and 1 <= args.timeout <= 300, 'invalid repeat count or timeout')
        if args.list:
            for row in validate(load(args.spec)):
                print(row['id'])
            return 0
        return run(args)
    except (ValueError, OSError, KeyError, TypeError) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
