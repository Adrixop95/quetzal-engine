#!/usr/bin/env python3
"""Smoke-test an extracted engine package using only invented data."""
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile


def main():
    package = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix='quetzal package ') as directory:
        root = Path(directory)
        relocated = root / 'relocated package'
        shutil.copytree(package, relocated)
        executable = relocated / ('aztec_engine.exe' if os.name == 'nt' else 'aztec_engine')
        game = root / 'private synthetic game'
        maps = game / 'Maps'
        maps.mkdir(parents=True)
        res = game / 'Res'
        res.mkdir()
        for name in ('Pal', 'Units', 'Tiles0', 'Tiles1', 'common', 'fnts', 'light'):
            (res / f'{name}.xbe').write_bytes(struct.pack('<I', 0))
        (maps / 'mission_1.mp').write_text(
            '[Globals]\nMapSize=8,8\nSides=1\nTeams=1\n'
            '[Side0]\nStart_X=1\nStart_Y=1\n'
            '[Team0]\nSide=0\nUnits=1\nUnit0=1,1,3,100\n', encoding='ascii')
        terrain = maps / 'mission_1.map'
        terrain.write_bytes(struct.pack('<HH64I', 8, 8, *([0] * 64)))
        (maps / 'mission_1.scn').write_text(
            'trigger(0) conditions localTime == 10 actions side(local).victory end\n', encoding='ascii')
        env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', AZTEC_GAME=str(game))
        env.pop('LD_LIBRARY_PATH', None)
        if os.name == 'nt':
            # Exclude vcpkg/compiler locations so missing packaged DLLs cannot hide on PATH.
            env['PATH'] = str(Path(os.environ['SystemRoot']) / 'System32')
        work = root / 'unrelated working directory'
        work.mkdir()

        if not (relocated / 'LOCAL-ONLY.txt').exists():
            if os.name == 'nt':
                assert (relocated / 'licenses/SDL2.txt').is_file()
                assert (relocated / 'SDL2-source.tar.gz').is_file()
            else:
                assert (relocated / 'dependencies.json').is_file()
                assert list((relocated / 'dependency-sources').glob('*.dsc'))
                linked = subprocess.check_output(
                    ['ldd', str(relocated / 'aztec_engine.bin')], text=True,
                    env=dict(env, LD_LIBRARY_PATH=str(relocated / 'lib')), timeout=10)
                assert 'not found' not in linked, linked
                for name, filename in re.findall(r'^\s*(\S+) => (.+?) \(', linked, re.MULTILINE):
                    if re.fullmatch(r'lib(c|m|pthread|dl|rt|util|resolv)\.so\.[0-9]+', name):
                        continue
                    assert Path(filename).resolve().parent == relocated / 'lib', linked

        def run(*args, code=0):
            result = subprocess.run([str(executable), *args], cwd=work, env=env,
                                    capture_output=True, text=True, timeout=20)
            if result.returncode != code:
                raise AssertionError(f'{args}: {result.returncode}\n{result.stdout}\n{result.stderr}')
            return result

        def headless(*args, code=0):
            return json.loads(run('--headless', '--max-ticks', '20', '--no-auto-orders',
                                  *args, '1', code=code).stdout)

        assert 'usage:' in run('--help').stdout
        first = headless()
        assert first['status'] == 'victory' and first['ticks'] == 10, first
        assert first == headless(), 'synthetic replay differs'
        # Exercise SDL initialization, rendering and orderly shutdown as well as simulation.
        run('--game', str(game), '--no-audio', '--no-sprites', '--no-panel', '--no-light',
            '--software', '--frames', '2', '--screenshot', str(root / 'smoke.png'), '1')
        assert (root / 'smoke.png').is_file()
        missing = headless('--game', str(root / 'missing installation'), code=1)
        assert 'Res/Pal.xbe' in missing['error'], missing
        assert 'Res/Pal.xbe' in run('--game', str(root / 'missing installation'),
                                   '--no-menu', code=1).stderr
        pal = res / 'Pal.xbe'
        pal.write_bytes(b'bad')
        assert 'Res/Pal.xbe' in headless(code=1)['error']
        pal.write_bytes(struct.pack('<I', 0))
        terrain.write_bytes(b'bad')
        assert 'cannot load .map' in headless(code=1)['error']
        terrain.write_bytes(struct.pack('<HH64I', 8, 8, *([0] * 64)))
        assert 'cannot load rules' in headless('--rules-dir', str(root / 'missing rules'), code=1)['error']
        types = relocated / 'rules/types.csv'
        assert types.is_file()
        types.unlink()
        assert 'cannot load rules' in headless(code=1)['error']
        assert (relocated / 'LICENSE').is_file()
    print('Packaged engine: synthetic victory, repeatability, SDL and input errors passed')


if __name__ == '__main__':
    main()
