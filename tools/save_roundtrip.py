#!/usr/bin/env python3
"""Compare campaign save continuation in isolated fresh engine processes.

Private reports, saves and per-tick hashes stay under this worktree's work/.
This launches only the independent engine, never the original executable.
"""
import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))
from playthrough import ROOT, discover, paired_hashes, positive, private_game, run_once


def launch(args, mission, game, directory, **options):
    previous = os.environ.get('AZTEC_USER_DIR')
    os.environ['AZTEC_USER_DIR'] = str(directory / 'profile')
    try:
        return run_once(args, mission, game, directory, **options)
    finally:
        if previous is None:
            os.environ.pop('AZTEC_USER_DIR', None)
        else:
            os.environ['AZTEC_USER_DIR'] = previous


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--engine', type=Path, default=ROOT / 'build/aztec_engine')
    parser.add_argument('--game', type=Path, required=True)
    parser.add_argument('--rules-dir', type=Path, default=ROOT / 'docs/data')
    parser.add_argument('--orders-dir', type=Path, required=True,
                        help='private mission_N.orders scripts; required for every requested mission')
    parser.add_argument('--missions', type=positive, nargs='+', default=list(range(1, 16)))
    parser.add_argument('--max-ticks', type=positive, default=2400)
    parser.add_argument('--save-ticks', type=positive, nargs='+', default=[1, 120, 600, 1200])
    parser.add_argument('--timeout', type=positive, default=180)
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    try:
        for name in ('engine', 'game', 'rules_dir', 'orders_dir'):
            setattr(args, name, getattr(args, name).resolve())
        if set(args.missions) - set(range(1, 16)) or len(set(args.missions)) != len(args.missions):
            raise ValueError('missions must be distinct campaign numbers 1 through 15')
        if any(t >= args.max_ticks for t in args.save_ticks) or len(set(args.save_ticks)) != len(args.save_ticks):
            raise ValueError('save ticks must be distinct and precede max-ticks')
        maps, missions = discover(args.game)
        if set(args.missions) - missions.keys():
            raise ValueError('requested campaign mission is missing')
        for mission in args.missions:
            if not (args.orders_dir / f'mission_{mission}.orders').is_file():
                raise ValueError(f'missing mission_{mission}.orders')
        stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
        output = (args.out or ROOT / 'work' / f'save-roundtrip-{stamp}').resolve()
        private_root = (ROOT / 'work').resolve()
        if output == private_root or not output.is_relative_to(private_root):
            raise ValueError('--out must be a new directory under this worktree work/')
        for source in (args.game, args.rules_dir, args.orders_dir, args.engine):
            if output.is_relative_to(source) or source.is_relative_to(output):
                raise ValueError('output must not overlap inputs')
        output.mkdir(parents=True, exist_ok=False)
        shutil.copy2(args.engine, output / 'aztec_engine')
        shutil.copytree(args.rules_dir, output / 'rules')
        shutil.copytree(args.orders_dir, output / 'orders')
        args.engine, args.rules_dir, args.orders_dir = output / 'aztec_engine', output / 'rules', output / 'orders'
        args.no_auto_orders, args.order_interval = True, 120
        results = []
        for mission in args.missions:
            for tick in args.save_ticks:
                case = output / f'mission-{mission}-tick-{tick}'
                case.mkdir()
                # A process gets its own Maps/Saves/profile files. Asset links are read-only inputs.
                games = [case / f'game-{i}' for i in range(2)]
                for game in games:
                    private_game(args.game, maps, game)
                original = launch(args, mission, games[0], case / 'record', save_tick=tick)
                save = case / 'record/midmission.azs'
                resumed = launch(args, mission, games[1], case / 'resume', resume=save) if save.is_file() else {}
                comparison = paired_hashes(original, resumed, tick)
                # Early mission termination is recorded as incomplete, never a full N-tick success.
                reached = all(r.get('engine', {}).get('ticks') == args.max_ticks for r in (original, resumed))
                row = dict(mission=mission, save_tick=tick, record=original, resume=resumed,
                           comparison=comparison, horizon_reached=reached, passed=comparison.get('equal') is True and reached)
                results.append(row)
                (output / 'results.json').write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
                print(f'mission {mission}, save {tick}: {"passed" if row["passed"] else "failed or incomplete"}', flush=True)
        return 0 if all(row['passed'] for row in results) else 1
    except (OSError, ValueError) as error:
        print(f'save round-trip: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
