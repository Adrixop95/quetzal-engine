#!/usr/bin/env python3
"""Restart the synthetic preference client; no game data or display needed."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='aztec-profiles-') as temporary:
    root = Path(temporary)
    env = dict(os.environ, AZTEC_USER_DIR=str(root / 'user'))
    for mode in ('write', 'read'):
        subprocess.run([str(binary), mode, str(root / 'modifications.ini')],
                       env=env, check=True, timeout=15)
