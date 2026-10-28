#!/usr/bin/env python3
"""Stage only allowlisted independent-engine files and redistributable dependencies.

Linux release mode requires Debian package metadata and enabled deb-src indexes.
The local mode stages files for development smoke checks, without redistribution.
"""
import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def linux_runtime(binary, dest):
    libraries = dest / 'lib'
    libraries.mkdir()
    licenses = dest / 'licenses'
    sources = dest / 'dependency-sources'
    sources.mkdir()
    shutil.copytree('/usr/share/common-licenses', licenses / 'common-licenses')
    packages = {}
    dependencies = output('ldd', str(binary))
    if 'not found' in dependencies:
        raise RuntimeError(dependencies)
    for name, filename in re.findall(r'^\s*(\S+) => (/\S+)', dependencies, re.MULTILINE):
        # The ELF loader and glibc are supplied by the supported host OS.
        if re.fullmatch(r'lib(c|m|pthread|dl|rt|util|resolv)\.so\.[0-9]+', name):
            continue
        path = Path(filename)
        shutil.copy2(path.resolve(), libraries / name)
        owner = None
        for candidate in (str(path), str(path.resolve()), str(path).removeprefix('/usr')):
            result = subprocess.run(['dpkg-query', '-S', candidate], capture_output=True, text=True)
            if result.returncode == 0:
                owner = result.stdout.split(': ', 1)[0].splitlines()[0]
                break
        if not owner:
            raise RuntimeError(f'No Debian license/source metadata for {path}')
        if owner in packages:
            continue
        source, version = output('dpkg-query', '-W', '-f=${source:Package}\t${source:Version}', owner).split('\t')
        copyright_file = Path('/usr/share/doc') / owner.split(':')[0] / 'copyright'
        shutil.copy2(copyright_file, licenses / f'{owner.replace(":", "-")}.copyright')
        packages[owner] = {'source': source, 'version': version}
    # Preserve exact dependency sources beside their binaries, including distro patches.
    for source, version in sorted({(p['source'], p['version']) for p in packages.values()}):
        subprocess.run(['apt-get', 'source', '--download-only', '--only-source', f'{source}={version}'],
                       cwd=sources, check=True)
    (dest / 'dependencies.json').write_text(json.dumps(packages, indent=2) + '\n', encoding='utf-8')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--dest', required=True, type=Path)
    parser.add_argument('--runtime', required=True, choices=['debian', 'windows-static', 'local'])
    parser.add_argument('--sdl-source', type=Path)
    parser.add_argument('--sdl-license', type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    dest = args.dest.resolve()
    if dest.exists():
        parser.error('destination must not exist; use a fresh staging directory')
    if args.runtime == 'windows-static' and not (args.sdl_source and args.sdl_license):
        parser.error('Windows releases require SDL source and license paths')
    dest.mkdir(parents=True)
    (dest / 'licenses').mkdir()
    windows = binary.suffix.lower() == '.exe'
    target = dest / ('aztec_engine.exe' if windows else 'aztec_engine.bin')
    shutil.copy2(binary, target)
    if not windows:
        launcher = dest / 'aztec_engine'
        launcher.write_text('#!/bin/sh\nset -eu\n'
                            'package_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                            'export LD_LIBRARY_PATH="$package_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"\n'
                            'exec "$package_dir/aztec_engine.bin" "$@"\n', encoding='utf-8')
        launcher.chmod(0o755)
    shutil.copy2(ROOT / 'LICENSE', dest / 'LICENSE')
    shutil.copy2(ROOT / 'docs/PACKAGES.md', dest / 'README.md')
    # Rules are factual metadata already tracked in the public repository.
    (dest / 'rules').mkdir()
    tracked_rules = output('git', '-C', str(ROOT), 'ls-files', 'docs/data/*.csv', 'docs/data/*.json')
    for filename in tracked_rules.splitlines():
        shutil.copy2(ROOT / filename, dest / 'rules' / Path(filename).name)
    if args.runtime == 'debian':
        linux_runtime(binary, dest)
    elif args.runtime == 'windows-static':
        shutil.copy2(args.sdl_license, dest / 'licenses/SDL2.txt')
        shutil.make_archive(str(dest / 'SDL2-source'), 'gztar', args.sdl_source)
    else:
        (dest / 'LOCAL-ONLY.txt').write_text('Development staging only; runtime dependencies are not bundled.\n')
    print(dest)


if __name__ == '__main__':
    main()
