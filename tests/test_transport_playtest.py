"""Run real UI ferry clicks with invented archives, terrain and units."""
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

engine = str(Path(sys.argv[1]).resolve())
repo = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="aztec-transport-") as directory:
    root = Path(directory)
    (root / "Res").mkdir()
    (root / "Maps").mkdir()
    for archive in ("Pal", "Units", "Tiles0", "Tiles1", "common", "fnts", "light"):
        (root / "Res" / f"{archive}.xbe").write_bytes(bytes(4))
    base = root / "Maps" / "transport"
    base.with_suffix(".mp").write_text(
        "[Globals]\nMapSize=16,16\nSides=1\nTeams=1\nBoats=1\n"
        "[Side0]\nStart_X=7\nStart_Y=7\n[Team0]\nSide=0\nUnits=1\n"
        "Unit0=2,3,0,100\n[Boat0]\nSide=0\nX=5\nY=7\n", encoding="ascii")
    base.with_suffix(".map").write_bytes(struct.pack("<HH", 16, 16) + bytes(256 * 4))
    base.with_suffix(".dat").write_bytes(bytes(0 if 5 <= x <= 10 else 2
                                              for y in range(16) for x in range(16)))
    result = subprocess.run(
        [engine, "--game", str(root), "transport", "--rules-dir", str(repo / "docs/data"),
         "--script", str(repo / "tests/playtest/transport.txt"), "--size", "1000x768",
         "--cam", "7,7", "--zoom", "1", "--no-panel", "--no-fog", "--no-audio", "--no-light"],
        env=dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy",
                 AZTEC_USER_DIR=str(root / "profile")),
        capture_output=True, text=True, timeout=45)
    assert result.returncode == 0, result.stdout + result.stderr
    assert "FAIL" not in result.stdout, result.stdout
    print(result.stdout)
