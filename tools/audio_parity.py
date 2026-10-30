#!/usr/bin/env python3
"""L6 playback-request normalization, ordered comparison and isolated private probe."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys

# Reuse the existing private-copy/isolation policy, including fail-closed setup.
sys.path.insert(0, str(Path(__file__).resolve().parent))
from parity_report import execute, json_object, private_game, private_output, write_json

CLASSES = ('selection', 'orders', 'combat', 'building', 'ui', 'music', 'ambient')
KINDS = ('sound', 'loop_start', 'loop_stop', 'music', 'music_stop', 'speech', 'subtitle')


def integer(value, minimum=0):
    return type(value) is int and value >= minimum


def validate(document):
    if not isinstance(document, dict):
        raise ValueError('audio capture must be an object')
    header = document.get('header', {})
    events = document.get('events')
    if (not isinstance(header, dict) or type(header.get('schema')) is not int
            or header['schema'] != 1 or header.get('complete') is not True
            or any(not isinstance(header.get(k), str) or not header[k]
                   for k in ('scenario', 'contract'))
            or not integer(header.get('first_tick')) or not integer(header.get('last_tick'))
            or header['last_tick'] < header['first_tick']
            or not integer(document.get('event_count')) or not isinstance(events, list)
            or document['event_count'] != len(events)):
        raise ValueError('invalid or incomplete audio capture')
    last = header['first_tick']
    for index, event in enumerate(events):
        if (not isinstance(event, dict) or event.get('seq') != index
                or not integer(event.get('seq')) or not integer(event.get('tick'))
                or not last <= event['tick'] <= header['last_tick']
                or not integer(event.get('sound'), 1) or event.get('kind') not in KINDS
                or event.get('trigger') not in CLASSES):
            raise ValueError(f'invalid/unresolved event {index}; audit sound identity, trigger and tick alignment')
        last = event['tick']
    return document


def normalize(raw, manifest, source):
    """An audited sidecar binds annotations and clock contract to exact raw bytes.

    No event can be silently dropped. Reference buffer IDs never become WAV IDs.
    Completeness and scenario identity must be established by the scenario driver.
    """
    if source not in ('reference', 'clone'):
        raise ValueError('invalid audio source')
    data = raw.read_bytes()
    if manifest.get('raw_sha256') != hashlib.sha256(data).hexdigest():
        raise ValueError('manifest does not describe these raw bytes')
    if type(manifest.get('tick_offset')) is not int:
        raise ValueError('explicit audited tick_offset required')
    annotations = manifest.get('annotations', {})
    if not isinstance(annotations, dict):
        raise ValueError('annotations must be keyed by decimal sequence number')
    events = []
    for index, line in enumerate(data.decode().splitlines()):
        row = json_object(line)
        if not integer(row.get('seq')) or row['seq'] != index or not integer(row.get('tick')):
            raise ValueError('raw sequence gap or invalid tick')
        annotation = annotations.get(str(index), {})
        if not isinstance(annotation, dict):
            raise ValueError('invalid annotation')
        if source == 'reference':
            if row.get('kind') not in ('play', 'stop', 'release') or row.get('sound') is not None:
                raise ValueError('expected reference shim capture')
            # The shim lacks resource IDs and causal triggers; require every annotation.
            if set(annotation) != {'sound', 'kind', 'trigger'}:
                raise ValueError(f'reference event {index} needs sound, kind and trigger evidence')
            event = dict(annotation)
        else:
            if set(annotation) - {'trigger'} or (annotation and row.get('trigger') != 'unknown'):
                raise ValueError('clone annotations may only resolve unknown triggers')
            event = {k: row.get(k) for k in ('sound', 'kind', 'trigger')}
            event.update(annotation)
        event.update(seq=index, tick=row['tick'] + manifest['tick_offset'])
        events.append(event)
    if set(annotations) - {str(i) for i in range(len(events))}:
        raise ValueError('annotation beyond captured event sequence')
    return validate(dict(header=manifest.get('header'), event_count=len(events), events=events))


def compare(reference, clone):
    validate(reference)
    validate(clone)
    if reference['header'] != clone['header']:
        raise ValueError('scenario, clock/identity contract or capture horizons differ')
    left, right = reference['events'], clone['events']
    classes = {c: dict(reference=sum(e['trigger'] == c for e in left),
                       clone=sum(e['trigger'] == c for e in right), differences=0) for c in CLASSES}
    differences = []
    # Compare the GLOBAL sequence. Sorting/grouping by class would hide reordering.
    for index in range(max(len(left), len(right))):
        a = left[index] if index < len(left) else None
        b = right[index] if index < len(right) else None
        category = None
        if a is None:
            category = 'extra_event'
        elif b is None:
            category = 'missing_event'
        elif any(a[k] != b[k] for k in ('sound', 'kind', 'trigger')):
            category = 'identity_or_order'
        elif abs(a['tick'] - b['tick']) > 1:
            category = 'timing'
        if category:
            affected = sorted({e['trigger'] for e in (a, b) if e is not None})
            for trigger in affected:
                classes[trigger]['differences'] += 1
            differences.append(dict(index=index, category=category, triggers=affected, reference=a, clone=b))
    for row in classes.values():
        row['status'] = ('different' if row['differences'] else
                         'compared' if row['reference'] or row['clone'] else 'unobserved')
    return dict(category='different' if differences else 'equal_observed_events' if left else 'no_events',
                tolerance_ticks=1, classes=classes, first=differences[0] if differences else None,
                differences=differences)


def probe(args):
    output = private_output(args.output)
    output.mkdir(parents=True, exist_ok=False)
    if not (args.game / 'Aztec.exe').is_file() or not args.runtime.is_file():
        write_json(output / 'report.json', dict(category='skipped_missing_private_inputs',
                   classes={c: 'unmeasured' for c in CLASSES}))
        return 77
    private_game(args.game.resolve(), output / 'game')
    script = output / 'input.txt'
    if args.script:
        shutil.copyfile(args.script, script)
    else:
        script.write_text('wait 3000\nquit\n')
    category = 'captured_unmapped'
    try:
        execute([args.runtime.resolve(), output / 'game/Aztec.exe'], output, args.timeout,
                dict(RT_AUDIO_LOG=str(output / 'reference.jsonl'), RT_SCRIPT=str(script),
                     RT_NO_GRAB='1', RT_NO_VIDEO='1', RT_RESOLUTION='800x600'))
        if not (output / 'reference.jsonl').is_file():
            category = 'blocked_missing_audio_hook_or_no_requests'
    except (RuntimeError, OSError) as error:
        write_json(output / 'report.json', dict(category='experiment_failed', reason=str(error),
                   classes={c: 'unmeasured' for c in CLASSES}))
        return 1
    write_json(output / 'report.json', dict(category=category,
               classes={c: 'unmeasured' for c in CLASSES}))
    return 1  # Raw capture alone never establishes parity.


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    norm = commands.add_parser('normalize')
    norm.add_argument('source', choices=('reference', 'clone'))
    norm.add_argument('raw', type=Path)
    norm.add_argument('manifest', type=Path)
    diff = commands.add_parser('compare')
    diff.add_argument('reference', type=Path)
    diff.add_argument('clone', type=Path)
    run = commands.add_parser('probe')
    run.add_argument('--game', required=True, type=Path)
    run.add_argument('--runtime', required=True, type=Path)
    run.add_argument('--script', type=Path)
    run.add_argument('--timeout', type=int, default=30)
    for command in (norm, diff, run):
        command.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    try:
        if args.command == 'probe':
            if args.timeout <= 0:
                raise ValueError('timeout must be positive')
            return probe(args)
        output = private_output(args.output)
        # Exclusive creation prevents overwriting captures or earlier findings.
        output.parent.mkdir(parents=True, exist_ok=True)
        if args.command == 'normalize':
            result = normalize(args.raw, json_object(args.manifest.read_text()), args.source)
        else:
            result = compare(json_object(args.reference.read_text()), json_object(args.clone.read_text()))
        with output.open('x') as stream:
            json.dump(result, stream, indent=2, sort_keys=True)
            stream.write('\n')
        return int(args.command == 'compare' and result['category'] != 'equal_observed_events')
    except (OSError, ValueError, TypeError, KeyError) as error:
        parser.exit(2, f'audio parity: {error}\n')


if __name__ == '__main__':
    sys.exit(main())
