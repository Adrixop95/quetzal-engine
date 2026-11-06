#!/usr/bin/env python3
"""Compare complete observed UI routes, ignoring repeated presentations of one screen.

Reference input requires an audited task-64 adapter. Scenario labels and screenshot
filenames are not observations and must never be used to manufacture this stream.
"""
import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from parity_report import private_output, write_json


def sequence(path, origin):
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    if not rows:
        raise ValueError('empty UI observation stream')
    result = []
    for frame, row in enumerate(rows):
        contract = row.get('contract')
        allowed = ('engine-menu-raw-v1', 'engine-ui-raw-v1') if origin == 'clone' else ('reference-ui-raw-v1',)
        if (contract not in allowed or type(row.get('frame')) is not int or row['frame'] != frame
                or row.get('phase') != 'after_input_before_present'):
            raise ValueError(f'invalid {origin} UI observation at frame {frame}')
        screen = row.get('rendered_screen') if contract == 'engine-menu-raw-v1' else row.get('screen')
        if not isinstance(screen, str) or not screen or any(c not in 'abcdefghijklmnopqrstuvwxyz0123456789-' for c in screen):
            raise ValueError(f'missing or invalid observed screen at frame {frame}')
        if not result or result[-1]['screen'] != screen:
            result.append(dict(frame=frame, screen=screen))
    # The caller supplies the declared horizon; a nonempty prefix is not a full route.
    return result, len(rows)


def compare(reference, clone, reference_frames, clone_frames):
    if any(type(count) is not int or count < 1 for count in (reference_frames, clone_frames)):
        raise ValueError('positive declared frame horizons required')
    left, left_frames = sequence(reference, 'reference')
    right, right_frames = sequence(clone, 'clone')
    if left_frames != reference_frames or right_frames != clone_frames:
        raise ValueError('UI route ended before or after its declared horizon')
    for index in range(max(len(left), len(right))):
        a = left[index] if index < len(left) else None
        b = right[index] if index < len(right) else None
        if a is None or b is None or a['screen'] != b['screen']:
            return dict(status='different', transition=index, reference=a, clone=b)
    return dict(status='screen-sequence-equal', transitions=len(left),
                note='Screen order only; input timing, pixels, sound and simulation parity are unmeasured.')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference', required=True, type=Path)
    parser.add_argument('--clone', required=True, type=Path)
    parser.add_argument('--reference-frames', required=True, type=int)
    parser.add_argument('--clone-frames', required=True, type=int)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    output = private_output(args.output)
    if output.exists():
        parser.error('use a fresh private output directory')
    output.mkdir(parents=True)
    try:
        result = compare(args.reference, args.clone, args.reference_frames, args.clone_frames)
    except (ValueError, OSError, KeyError, TypeError) as error:
        result = dict(status='needs live run', error=str(error))
    write_json(output / 'sequence.json', result)
    print(json.dumps(result, indent=2))
    return 0 if result['status'] == 'screen-sequence-equal' else 1


if __name__ == '__main__':
    sys.exit(main())
