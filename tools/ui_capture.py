#!/usr/bin/env python3
"""Capture front-end routes privately; distinguish repeatability from parity."""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from parity_report import ROOT, execute, private_game, private_output, write_json
from parity_visual import image_file, require


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
             '--menu-trace', directory / 'trace.jsonl'], directory, args.timeout, setup_network=False)
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


def run(args):
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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scenario', type=Path, default=ROOT / 'tests/parity/ui/frontend.json')
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--clone', type=Path, default=ROOT / 'build/aztec_engine')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--timeout', type=int, default=30)
    args = parser.parse_args()
    try:
        require(1 <= args.timeout <= 300, 'timeout must be 1..300 seconds')
        return run(args)
    except (ValueError, OSError, KeyError, TypeError) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
