"""Fresh-process save continuation and a real short-write fault, using synthetic data."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def run(*args, **kwargs):
    subprocess.run([sys.argv[1], *args], check=True, timeout=45, **kwargs)


with tempfile.TemporaryDirectory(prefix="aztec-engine-save-") as directory:
    slot = Path(directory) / "slot.azs"
    run("--write", str(slot))
    saved = slot.read_bytes()
    run("--resume", str(slot))
    assert slot.read_bytes() == saved
    if os.name == "posix":
        import resource
        import signal

        def limit_file_size():
            resource.setrlimit(resource.RLIMIT_FSIZE, (512, 512))
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)

        run("--expect-write-failure", str(slot), preexec_fn=limit_file_size)
        assert slot.read_bytes() == saved
        assert list(Path(directory).iterdir()) == [slot]
