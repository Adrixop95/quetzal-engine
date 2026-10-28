# Quetzal Engine binary packages

Download the archive for your platform and extract it into a writable directory.
Packages contain the independent engine, public rules metadata, GPL-3.0 license,
dependency notices and dependency sources. They contain no original game data,
reference runtime or generated game code. The engine is still incomplete; a
successful package smoke check does not establish campaign playability.

Linux x86_64 packages target Ubuntu 24.04 or newer compatible systems (glibc
2.39 or newer). Run `./aztec_engine`; keep the launcher, binary, lib and rules
in the same directory. Shared dependencies are bundled except the host ELF
loader and glibc. Display/audio drivers, a desktop session and graphics driver
integration come from the operating system. Windows x86_64 packages target
Windows 10/11 and statically link SDL2 and the MSVC runtime; no separate SDL2.dll
or Visual C++ runtime installation is required. Keep rules beside the executable.

## Supply your own game installation

Use a legally obtained Aztec Empire installation. No game download, extraction
or executable patch is supplied. Keep game data outside the package directory.
The engine requires `Res/Pal.xbe`, `Units.xbe`, `Tiles0.xbe`, `Tiles1.xbe`,
`common.xbe`, `fnts.xbe` and `light.xbe`, plus `Maps/` mission files (`.mp`,
`.map`, `.scn` and related resources). Preserve the installation directory
structure. Music, Video, configuration and profile files enable optional
features. The local original executable is optional for daylight lookup; it is
never distributed by this project. Missing files produce a path-specific error;
replace malformed files from your own installation.

On Linux, from any working directory:

```sh
/path/to/package/aztec_engine --game "/path/to/Aztec Empire" 1
```

On Windows PowerShell:

```powershell
& 'C:\Quetzal Engine\aztec_engine.exe' --game 'D:\Games\Aztec Empire' 1
```

Omit `1` to open the front end. Alternatively set `AZTEC_GAME` to the installation
path. `--rules-dir DIR` overrides the bundled rules. Paths may contain spaces.
Use a private writable game copy for saves/settings; do not share writable state
between engine/reference processes. See docs/EXPERIMENTS.md in the source archive
for private read-only asset symlinks and isolated reference experiments.

Intro video playback optionally invokes an external `ffmpeg` on PATH. ffmpeg is
not bundled or required to load/play missions; install it separately if desired.
Native audio/video hardware and real game resources require separate testing.

## Licenses and source

Engine code and public metadata are distributed under the accompanying GPL-3.0
license. Original game assets retain their owners' rights and are not covered by
that license. `licenses/` contains bundled dependency notices. Linux includes
`dependencies.json` with exact Debian source versions and `dependency-sources/`
with original source archives and distribution patches. Windows includes SDL2's
zlib license and `SDL2-source.tar.gz`; the MSVC runtime is linked under Microsoft's
Visual Studio redistribution terms. ffmpeg remains a separate optional program.

Each published release also includes `quetzal-engine-source.tar.gz`, containing
the exact public engine source and build/packaging instructions for that release.
Retain corresponding source and notices when redistributing packages. Build
instructions are in docs/SETUP.md; the release recipe is in
.github/workflows/release.yml and tools/package_release.py.

## Package validation

The release workflow requires Linux/Windows builds, synthetic CTest and
pre-commit. Separate fresh jobs extract the archives and run a bounded synthetic
mission to victory, repeat it for determinism, exercise SDL dummy rendering, and
check missing/corrupt resource and terrain errors from an unrelated directory.
These checks use no oracle, generated source, original executable or game assets.
Local developer staging with `--runtime local` omits dependency bundling and is
explicitly marked LOCAL-ONLY; it must never be published.
