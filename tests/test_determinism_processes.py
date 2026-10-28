"""Compare every tick across fresh processes, views, presentation rates and save continuation."""
import os
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile


def compare(left, right, start=1):
    expected = left.read_text().splitlines()[start - 1:]
    actual = right.read_text().splitlines()
    if len(expected) != len(actual):
        raise RuntimeError(f"Truncated trace: {len(expected)} != {len(actual)}")
    for tick, (a, b) in enumerate(zip(expected, actual), start):
        if a != b:
            raise RuntimeError(f"First divergence at tick {tick}:\n{a}\n{b}")


def run(binary):
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        save = root / "continuation.azs"

        def launch(mode, name, view, frames):
            trace = root / name
            subprocess.run([binary, mode, str(trace), str(save), str(view), str(frames)],
                           env=env, check=True, timeout=45)
            return trace

        reference = launch("record", "reference", 0, 0)
        require_ticks = reference.read_text().splitlines()
        if len(require_ticks) != 14400 or require_ticks[-1].split()[0] != "14400":
            raise RuntimeError("Long replay did not reach its horizon")
        compare(reference, launch("repeat", "repeat", 1, 5))
        compare(reference, launch("resume", "resume", 7, 2), start=7202)
        compare(reference, launch("log-replay", "log-replay", 3, 0))
        exported = os.environ.get("AZTEC_DETERMINISM_TRACE")
        if exported:
            shutil.copyfile(reference, exported)
        damaged = bytearray(save.read_bytes())
        if damaged[:4] != b"AZS3":
            raise RuntimeError("Unexpected save format")
        name_size = int.from_bytes(damaged[12:16], "little")
        damaged[16 + name_size + 8] ^= 1  # Saved state hash, after the mission string and tick.
        save.write_bytes(damaged)
        rejected = subprocess.run([binary, "resume", str(root / "rejected"), str(save), "0", "0"],
                                  env=env, capture_output=True, text=True, timeout=45)
        if rejected.returncode == 0 or "cannot read save" not in rejected.stderr:
            raise RuntimeError("Corrupt save checksum was not rejected")
        # Recompute the container checksum so replay must detect the changed state hash.
        checksum = 1469598103934665603
        for byte in damaged[:-8]:
            checksum = ((checksum ^ byte) * 1099511628211) & ((1 << 64) - 1)
        damaged[-8:] = checksum.to_bytes(8, "little")
        save.write_bytes(damaged)
        rejected = subprocess.run([binary, "resume", str(root / "rejected"), str(save), "0", "0"],
                                  env=env, capture_output=True, text=True, timeout=45)
        if rejected.returncode == 0 or "continuation hash mismatch" not in rejected.stderr:
            raise RuntimeError("Corrupt saved hash was not rejected")
        # The verifier itself must detect both differing and truncated controls.
        different = root / "different"
        different.write_text("changed\n" + "\n".join(require_ticks[1:]) + "\n")
        truncated = root / "truncated"
        truncated.write_text("\n".join(require_ticks[:-1]) + "\n")
        for trace in (different, truncated):
            try:
                compare(reference, trace)
            except (RuntimeError, UnicodeError):
                pass
            else:
                raise RuntimeError("Comparator accepted a negative control")
        print("14400 ticks agree across processes/views/presentation; fresh-process save continuation agrees")


if __name__ == "__main__":
    if sys.argv[1] == "--compare":
        for other in sys.argv[3:]:
            compare(Path(sys.argv[2]), Path(other))
    else:
        run(str(Path(sys.argv[1]).resolve()))
