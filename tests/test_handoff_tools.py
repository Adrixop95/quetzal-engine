#!/usr/bin/env python3
"""Portable queue and regeneration checks; no private game files or git writes.

Run: python3 -m unittest discover -s tests -p test_handoff_tools.py
"""
import contextlib
import importlib.util
import io
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("codex_orch", ROOT / "tools/codex_orch.py")
ORCH = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(ORCH)


class HandoffToolsTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copytree(ROOT / "tools", self.root / "tools", ignore=shutil.ignore_patterns("__pycache__"))
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ["PATH"])

    def stub(self, name, body):
        path = self.bin / name
        path.write_text("#!/bin/sh\n" + body + "\n")
        path.chmod(0o755)

    def script(self, name, *args):
        return subprocess.run(["sh", str(self.root / "tools" / name), *args],
                              cwd="/tmp", env=self.env, text=True, capture_output=True)

    def test_harvest_retry_and_failures(self):
        task = {"workdir": str(self.root)}
        (self.root / "work/codex").mkdir(parents=True)
        branch = "feature/codex-population"
        for failed_step in (None, "checkout", "git fetch", "git apply", "git add", "git diff", "git reset"):
            with self.subTest(failed_step=failed_step):
                commands = []
                output = io.StringIO()

                def run(cmd):
                    commands.append(cmd)
                    failure = failed_step and failed_step in cmd
                    text = "synthetic patch\n" if cmd == "git diff --cached --binary" else ""
                    return subprocess.CompletedProcess(cmd, int(bool(failure)), text,
                                                       "simulated failure" if failure else "")

                with patch.object(ORCH, "ROOT", str(self.root)), \
                     patch.object(ORCH, "load", return_value=dict(task)), \
                     patch.object(ORCH, "refresh", side_effect=lambda value: value), \
                     patch.object(ORCH, "save") as save, \
                     patch.object(ORCH, "sh", side_effect=run), \
                     patch.object(ORCH.os, "chdir"), contextlib.redirect_stdout(output):
                    if failed_step:
                        with self.assertRaisesRegex(RuntimeError, "simulated failure"):
                            ORCH.harvest("population", branch, "Population")
                        save.assert_not_called()
                        self.assertNotIn("prepared", output.getvalue())
                    else:
                        ORCH.harvest("population", branch, "Population")
                        self.assertTrue("git checkout -q -f -B " + branch in commands[-1])
                        self.assertTrue("git apply --3way" in commands[-1])
                        self.assertTrue("prepared with the patch applied" in output.getvalue())
                        self.assertTrue(save.call_args.args[0]["harvested"])
                        self.assertEqual((self.root / "work/codex/population.patch").read_text(),
                                         "synthetic patch\n")

    def test_queue_preference_and_legacy_fallback(self):
        legacy = self.root / "work/codex/queue"
        legacy.mkdir(parents=True)
        (legacy / "old.txt").write_text("Legacy prompt\n")
        with patch.object(ORCH, "ROOT", str(self.root)):
            self.assertEqual(Path(ORCH.queued_prompt("old")).parent, legacy)
            queue = self.root / "tasks/queue"
            queue.mkdir(parents=True)
            (queue / "new.md").write_text("Portable prompt\n")
            (queue / "ignored.md.started").write_text("Started\n")
            self.assertEqual(ORCH.queue_files(), [str(queue / "new.md")])
            self.assertEqual(ORCH.queued_prompt("new"), str(queue / "new.md"))
            with self.assertRaises(FileNotFoundError):
                ORCH.queued_prompt("old")

    def test_bootstrap_is_idempotent_and_can_skip_git_writes(self):
        for tool in ("git", "cmake", "g++", "python3", "objdump", "pkg-config", "ffmpeg"):
            self.stub(tool, "exit 0")
        marker = self.root / "hooks-called"
        self.stub("pre-commit", 'echo install >> "' + str(marker) + '"')
        for _ in range(2):
            result = self.script("bootstrap.sh", "--no-hooks")
            self.assertEqual(result.returncode, 0, result.stderr)
        self.assertFalse(marker.exists())
        for directory in ("build", "build_analysis", "work/gen", "work/decomp"):
            self.assertTrue((self.root / directory).is_dir())
        self.assertEqual(self.script("bootstrap.sh").returncode, 0)
        self.assertEqual(marker.read_text(), "install\n")
        self.stub("pkg-config", "exit 1")
        self.assertNotEqual(self.script("bootstrap.sh").returncode, 0)
        self.assertEqual(marker.read_text(), "install\n")

    def test_regeneration_missing_inputs_and_failure(self):
        for _ in range(2):
            result = self.script("regen_analysis.sh")
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue("SKIP executable analysis" in result.stdout)
        game = self.root / "game"
        game.mkdir()
        (game / "Aztec.exe").write_text("Synthetic placeholder, not a game executable\n")
        self.stub("objdump", "echo synthetic-disassembly")
        result = self.script("regen_analysis.sh")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue("gen_funcs.h requires" in result.stdout)
        self.assertTrue((self.root / "work/text.asm").is_file())
        self.stub("objdump", "exit 42")
        self.assertEqual(self.script("regen_analysis.sh").returncode, 42)
        self.assertEqual(self.script("regen_analysis.sh", "--ghidra").returncode, 2)

    def test_regeneration_cached_analysis_chain(self):
        # Invented PE metadata and a vtable store exercise readers without game code.
        game = self.root / "private game"
        game.mkdir()
        data = bytearray(0xa0000)
        pe = 0x40
        struct.pack_into("<I", data, 0x3c, pe)
        struct.pack_into("<H", data, pe + 6, 2)
        struct.pack_into("<H", data, pe + 20, 224)
        struct.pack_into("<I", data, pe + 52, 0x400000)
        for index, (name, address, offset) in enumerate(((b".text", 0x1000, 0x200),
                                                       (b".rdata", 0x8f000, 0x300))):
            section = pe + 24 + 224 + 40 * index
            data[section:section + len(name)] = name
            struct.pack_into("<IIII", data, section + 8, 16, address, 16, offset)
        data[0x200:0x206] = bytes.fromhex("c7 00 00 f0 48 00")
        struct.pack_into("<I", data, 0x300, 0x401000)
        struct.pack_into("<I", data, 0x933f8, 21)  # Stop the unrelated factory table scan.
        (game / "Aztec.exe").write_bytes(data)
        gen = self.root / "work/gen"
        gen.mkdir(parents=True)
        (gen / "gen_funcs.h").write_text("void f_00401000(CPU *cpu);\n")
        decomp = self.root / "work/decomp"
        decomp.mkdir()
        (decomp / "all.c").write_text("// ==== 0043d5d0 factory\n// ==== 00400000 other\n")
        survey = self.root / "work/survey/m1"
        survey.mkdir(parents=True)
        (survey / "objs_1.txt").write_text("10000000 0048f000 a1100000 000000b0 0 0\n")
        self.stub("objdump", "printf '  401000:\\tc7 00 00 f0 48 00 \\tmov DWORD PTR [eax],0x48f000\\n'")
        result = self.script("regen_analysis.sh", "--game", str(game))
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        for filename in ("classes.json", "class_edges.json", "ctor_sizes.json", "source_map.json",
                         "func_names.json", "vtables.txt", "classes.md", "object_kinds.txt",
                         "class_model.json", "classes_observed.md", "objects.hpp"):
            self.assertTrue((self.root / "work" / filename).is_file(), filename)
        self.assertFalse((self.root / "src").exists())
        self.assertFalse((self.root / "docs").exists())


if __name__ == "__main__":
    unittest.main()
