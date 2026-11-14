"""Fifteen invented scenarios, multiple save boundaries, fresh processes only."""
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
checkpoints = (0, 1, 2, 5, 20, 45, 60, 120, 240, 355, 399)
horizon = 400
with tempfile.TemporaryDirectory(prefix='quetzal-state-save-') as temporary:
    for mission in range(1, 16):
        root = Path(temporary) / str(mission)
        root.mkdir()
        for checkpoint in checkpoints:
            for mode in ('record', 'resume'):
                result = subprocess.run([binary, mode, str(root), str(mission), str(checkpoint), str(horizon)],
                                        capture_output=True, text=True, timeout=20)
                if result.returncode:
                    raise RuntimeError(f'{mission=} {checkpoint=} {mode=}: {result.stdout}{result.stderr}')
                if mode == 'record':
                    saved = (root / f'save-{checkpoint}.azs').read_bytes()
            assert saved == (root / f'save-{checkpoint}.azs').read_bytes()
            expected = (root / f'record-{checkpoint}.txt').read_text().splitlines()[checkpoint:]
            actual = (root / f'resume-{checkpoint}.txt').read_text().splitlines()
            if len(expected) != horizon - checkpoint + 1 or len(actual) != len(expected):
                raise RuntimeError(f'Truncated continuation: {mission=} {checkpoint=}')
            for tick, (left, right) in enumerate(zip(expected, actual), checkpoint):
                if left != right:
                    raise RuntimeError(f'First divergence: {mission=} {checkpoint=} {tick=}\n{left}\n{right}')
print('15 synthetic scenarios: all save boundaries and continued ticks match in fresh processes')
