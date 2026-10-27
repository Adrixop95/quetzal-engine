#!/usr/bin/env python3
"""Run isolated sprite-contract candidates; retain blocked/unsynchronized work as pending.

All generated maps, screenshots, dumps, logs and reports stay under ignored work/.
A successful process is only a capture, never automatic P evidence. Inspect the
active slots, overlap and measured daylight before interpreting comparisons.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--runtime', type=Path, required=True)
    parser.add_argument('--vtables', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'work/render-contract-probes')
    parser.add_argument('--night-time', type=int, default=5400)
    parser.add_argument('--row', type=int, default=318)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(ROOT / 'work'):
        parser.error('Output must be inside this worktree\'s ignored work/ directory')
    output.mkdir(parents=True, exist_ok=False)
    prerequisites = [args.game / 'Aztec.exe', args.game / 'Maps/mission_1.mp', args.runtime, args.vtables]
    missing = [str(p) for p in prerequisites if not p.exists()]
    cases = [
        ('attack_death', '3,30,7,33', {'OPP': '0', 'OPPDX': '1'},
         'Confirm non-Marine attack/death slots and orders; no-hit air is inconclusive'),
        ('overlap', '3,30,7,33', {'SPACING': '0'},
         'Confirm shared ground-Y/X order, aircraft altitude and shadows against bodies'),
        ('night', '0,1,3,30', {'START_TIME': str(args.night_time)},
         'Confirm actual ambient brightness, frame lights, vision and fractional positions'),
    ]
    records = []
    for name, types, overrides, question in cases:
        record = dict(probe=name, status='pending', evidence=None, question=question)
        if missing:
            record['blocked_by'] = missing
        else:
            env = dict(os.environ, GAME_DIR=str(args.game.resolve()), BIN=str(args.runtime.resolve()),
                       VTABLES=str(args.vtables.resolve()), SAMPLES='40', SAMPLE_SHOTS='1', SAMPLE_MS='83',
                       OPP='', SPACING='3', START_TIME='0')
            env.update(overrides)
            record['types'] = types
            record['controls'] = {key: env[key] for key in ('OPP', 'SPACING', 'START_TIME', 'SAMPLES', 'SAMPLE_MS')}
            cmd = ['sh', str(ROOT / 'tools/zspr_run.sh'), 'zcontract_' + name, types,
                   str(output / name), str(args.row), '1500']
            result = subprocess.run(cmd, cwd=ROOT, env=env, check=False)
            record['exit'] = result.returncode
            record['blocked_by'] = ('reference runner failed' if result.returncode else
                                    'capture requires stable brackets, matching active slots and pixel comparison')
        records.append(record)
    (output / 'inventory.json').write_text(json.dumps(records, indent=2) + '\n')
    return 2 if missing or any(r.get('exit', 0) for r in records) else 0


if __name__ == '__main__':
    raise SystemExit(main())
