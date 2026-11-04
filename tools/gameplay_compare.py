#!/usr/bin/env python3
"""Private gameplay recordings and strict L4/L5 timeline comparison.

Recording currently provides clone observations and a reference capability probe.
Presentation numbers are never substituted for reference simulation ticks.

Example (all outputs remain private)::

    python3 tools/gameplay_compare.py record --game /private/game \
        --runtime /private/build/aztec_recomp --every 120 \
        --output work/parity/ui/gameplay-run

The provisional coordinate route expresses intents, not verified emitted orders.
Clone captures observe completed ticks without consuming input frames; UI traces
include paused frames. Recording stops at the first mission result and reports
incomplete coverage. A reference launch still requires namespace network setup.
Task 58's deterministic reference tick/input and shared semantic UI observations
are prerequisites for a paired recording, even if its presentation probe succeeds.
Compare accepts the existing parity_visual specification and normalized bundles;
its reported coverage must be checked before claiming a ten-minute comparison.
Task routing is provisional until alignment/simulation causes have been excluded.
"""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from parity_report import ROOT, execute, private_game, private_output, write_json
from parity_visual import compare, image_file, load, require, validate_spec

SPEC = ROOT / 'tests/parity/ui/gameplay.json'
# Routing suggestions, not assertions that a pixel difference proves a root cause.
TASKS = {'visual_pixels': '59-ui-pixel-parity', 'visual_structure': '59-ui-pixel-parity',
         'ui_sequence': '61-ingame-ux-parity', 'commands': '11-oracle-parity-missions',
         'blocked_reference': '58-ui-capture-pipeline'}


def scenario(path):
    spec = load(path)
    require(spec.get('schema') == 1 and spec.get('evidence') == 'P', 'provisional scenario required')
    require(spec.get('input_clock') == 'clone-render-frame', 'input frame clock must be explicit')
    require(spec.get('missions') == [1, 2, 3] and spec.get('ticks') == 7200
            and spec.get('tick_rate') == 12 and spec.get('resolution') == [800, 600],
            'expected missions 1..3, ten minutes at 12 Hz, 800x600')
    events = spec.get('input')
    require(isinstance(events, list) and events, 'shared input required')
    previous = -1
    for event in events:
        require(isinstance(event, dict) and set(event) == {'frame', 'command', 'args', 'intent'},
                'invalid event fields')
        frame, command, values = event['frame'], event['command'], event['args']
        require(type(frame) is int and previous < frame < 7200 and frame >= 0, 'invalid frame schedule')
        previous = frame
        require(isinstance(event['intent'], str) and bool(event['intent']), 'input intent required')
        require(isinstance(values, list), 'event arguments required')
        if command == 'move':
            require(len(values) == 2 and all(type(v) is int for v in values)
                    and 0 <= values[0] < 800 and 0 <= values[1] < 600, 'invalid mouse position')
        elif command in ('keydown', 'keyup'):
            require(len(values) == 1 and values[0] in ('Right', 'Left', 'Up', 'Down', 'Escape', 'A', 'H'),
                    'unsupported shared key')
        else:
            require(command in ('down', 'up', 'rdown', 'rup') and not values, 'unsupported input')
    require({'selection', 'orders', 'building', 'training', 'scrolling', 'menu'}
            <= {event['intent'] for event in events}, 'all interaction intents required')
    return spec


def clone_script(spec):
    """Compile absolute input frames; capture commands never consume input frames."""
    lines, frame = [], 0
    for event in spec['input']:
        if event['frame'] > frame:
            lines.append(f"wait {event['frame'] - frame}")
        lines.append(' '.join(map(str, [event['command'], *event['args']])))
        frame = event['frame'] + 1
    lines.append(f"until {spec['ticks']}")
    lines.append('quit')
    return '\n'.join(lines) + '\n'


def reference_probe(mission):
    # Presentation-count navigation is a capability probe, never a gameplay clock.
    lines = []
    for frame, x, y in ((120, 140, 239), (180, 160, 543),
                        (240, 140, 257 + 20 * (mission - 1)), (300, 140, 544), (420, 700, 430)):
        lines += [f'{frame} move {x} {y}', f'{frame + 1} down', f'{frame + 2} up']
    return '\n'.join([*lines, '600 shot', '601 quit', ''])


def clone_record(args, spec, directory, mission):
    script = directory / 'input.txt'
    script.write_text(clone_script(spec))
    captures = directory / 'captures'
    captures.mkdir()
    failure = None
    try:
        execute([args.clone.resolve(), '--game', directory / 'game', str(mission),
                 '--rules-dir', ROOT / 'docs/data', '--size', '800x600', '--software', '--no-audio',
                 '--no-mouse-grab', '--no-edge-scroll', '--menu-session', '--fps', '10000',
                 '--script', script, '--ui-trace', directory / 'ui.jsonl',
                 '--capture-dir', captures, '--capture-every', args.every], directory, args.timeout,
                dict(AZTEC_USER_DIR=str(directory / 'profile'), AZTEC_UI_SCRIPT='', AZTEC_GAME=str(directory / 'game')),
                setup_network=False)
    except RuntimeError as exc:
        failure = str(exc)  # Preserve a valid prefix for diagnosis, never a completed run.
    rows = [json.loads(line) for line in (directory / 'ui.jsonl').read_text().splitlines()]
    require(rows and all(row['contract'] == 'engine-ui-raw-v1' and row['frame'] == i
                        for i, row in enumerate(rows)), 'truncated/invalid clone trace')
    last, observed = 0, []
    for row in rows:
        require(type(row['tick']) is int and last <= row['tick'] <= last + 1,
                'clone tick clock jumped or reset')
        if row['tick'] > last and row['tick'] % args.every == 0:
            shot = captures / f"tick-{row['tick']}.png"
            require(row['screenshot'] == str(shot), 'capture and UI observation differ')
            require(image_file(shot)[:3] == (800, 600, 3), 'invalid clone frame')
            observed.append(dict(tick=row['tick'], frame=row['frame'], image=str(shot)))
        last = row['tick']
    return dict(status='incomplete_clone_recording' if failure else 'unpaired_clone_recording',
                process_failure=failure, last_tick=last, outcome=rows[-1]['outcome'], frames=len(rows), captures=observed,
                complete=failure is None and last >= spec['ticks'], normalized_ui=False)


def draft(root, category, details):
    target = root / f'gameplay-{category}.md'
    require(not target.is_symlink(), 'draft must not be a symlink')
    root.mkdir(parents=True, exist_ok=True)
    if target.exists():
        return  # Preserve human edits; a fresh run's report remains authoritative.
    with target.open('x') as stream:
        stream.write(f"Draft follow-up for {TASKS[category]}\n\n"
                     "Read CLAUDE.md and docs/PARITY_TEST_PLAN.md. Keep all captures private.\n"
                     "This is a routing suggestion; establish the cause before changing behavior.\n"
                     "Require a successful isolated reference launch, audited tick/UI normalization,\n"
                     "and repeatable aligned recordings. Compare before and after with the same inputs.\n\n"
                     + json.dumps(details, indent=2) + '\n')


def record(args):
    spec = scenario(args.spec)
    output = private_output(args.output)
    require(output.is_relative_to((ROOT / 'work/parity/ui').resolve()), 'captures must stay in work/parity/ui')
    require(not output.exists(), 'use a fresh output directory')
    output.mkdir(parents=True)
    write_json(output / 'scenario.json', spec)
    reports = []
    for mission in spec['missions']:
        row = dict(screen=f'hud-mission-{mission}', runners={}, difference=None,
                   status='blocked_reference_tick_and_ui_contract')
        for runner in ('reference', 'clone'):
            directory = output / row['screen'] / runner
            directory.mkdir(parents=True)
            try:
                private_game(args.game.resolve(), directory / 'game')
                if runner == 'clone':
                    row['runners'][runner] = clone_record(args, spec, directory, mission)
                else:
                    script = directory / 'input.txt'
                    script.write_text(reference_probe(mission))
                    execute([args.runtime.resolve(), directory / 'game/Aztec.exe'], directory, args.timeout,
                            dict(RT_UI_SCRIPT=str(script), RT_NO_GRAB='1', RT_NO_VIDEO='1',
                                 RT_RESOLUTION='800x600', RT_DUMP_DIR=str(directory),
                                 RT_STATE_LOG=str(directory / 'state.txt'), RT_CMD_LOG=str(directory / 'commands.txt')))
                    require('UI capture contract: guest-present-v1' in (directory / 'process.log').read_text(),
                            'runtime lacks RT_UI_SCRIPT; rebuild privately')
                    image_file(directory / 'shot_01.ppm')
                    row['runners'][runner] = dict(status='unaligned_presentation_probe',
                                                 reason='no deterministic tick scheduler or semantic UI observer')
            except (ValueError, OSError, RuntimeError, KeyError) as exc:
                row['runners'][runner] = dict(status='experiment_failed', reason=str(exc))
        reports.append(row)
        write_json(output / 'report.json', reports)
        print(row['screen'] + ': ' + ', '.join(f'{r}={v["status"]}' for r, v in row['runners'].items()), flush=True)
    draft(private_output(ROOT / 'work/parity/tasks'), 'blocked_reference',
          dict(report=str(output / 'report.json'), screens=[r['screen'] for r in reports]))
    return 1  # Unpaired or incomplete recordings cannot pass L4/L5.


def compare_timeline(spec_path, reference, clone):
    """Use the existing audited alignment/mask/UI bundle contract, fail closed."""
    spec = load(spec_path)
    validate_spec(spec)
    report = compare(spec_path, reference, clone)
    groups = defaultdict(list)
    for sample in report['samples']:
        for region, metric in sample['regions'].items():
            if not metric['passed']:
                cause = 'visual_structure' if metric['mask_difference'] else 'visual_pixels'
                groups[cause].append(dict(tick=sample['tick'], screen=sample['step'], region=region, metrics=metric))
        for field in sample['ui_differences']:
            category = 'commands' if field == 'commands' else 'ui_sequence'
            groups[category].append(dict(tick=sample['tick'], screen=sample['step'], field=field))
    report['triage'] = {key: dict(task=TASKS[key], attribution='unverified',
                                count=len(rows), first=rows[0]) for key, rows in sorted(groups.items())}
    report['coverage'] = dict(samples=len(report['samples']), last_tick=spec['observe'][-1]['tick'])
    # These are logical observation steps; raw render frame numbers are runner-specific.
    if report['first'] is not None:
        report['first']['sample_index'] = next(i for i, row in enumerate(report['samples'])
                                              if row['tick'] == report['first']['tick'])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='action', required=True)
    p = commands.add_parser('record', help='clone recording plus isolated reference capability probe')
    p.add_argument('--spec', type=Path, default=SPEC)
    p.add_argument('--game', type=Path, required=True)
    p.add_argument('--runtime', type=Path, required=True)
    p.add_argument('--clone', type=Path, default=ROOT / 'build/aztec_engine')
    p.add_argument('--every', type=int, default=120)
    p.add_argument('--timeout', type=int, default=180)
    p.add_argument('--output', type=Path, required=True)
    p = commands.add_parser('compare', help='compare normalized, aligned timeline bundles')
    p.add_argument('scenario', type=Path)
    p.add_argument('reference', type=Path)
    p.add_argument('clone', type=Path)
    p.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.action == 'record':
            require(1 <= args.every <= 7200 and 1 <= args.timeout <= 900, 'invalid interval/timeout')
            return record(args)
        output = private_output(args.output)
        require(not output.exists(), 'use a fresh report path')
        output.parent.mkdir(parents=True, exist_ok=True)
        report = compare_timeline(args.scenario, args.reference, args.clone)
        write_json(output, report)
        for category, details in report['triage'].items():
            draft(private_output(ROOT / 'work/parity/tasks'), category, dict(report=str(output), **details))
        return int(report['first'] is not None)
    except (ValueError, RuntimeError, OSError, KeyError, TypeError) as exc:
        print(str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
