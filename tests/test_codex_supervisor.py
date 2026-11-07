#!/usr/bin/env python3
"""Supervisor regressions with fake gh, Codex and git runners; no network or git writes."""
import contextlib
import importlib
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
supervisor = importlib.import_module("codex_supervisor")
orch = importlib.import_module("codex_orch")
from codex_state import SupervisorState


def result(cmd, stdout="", code=0, stderr=""):
    return subprocess.CompletedProcess(cmd, code, stdout, stderr)


class FakeRunner:
    """Two open PRs: the older one varies, the newer one is always green."""
    def __init__(self, bucket="cancel", jobs=None):
        self.bucket = bucket
        self.jobs = jobs if jobs is not None else [{"conclusion": "cancelled", "steps": []}]
        self.commands = []
        self.mergeable = "MERGEABLE"
        self.rerun_code = 0
        self.check_code = 1
        self.behind = set()

    def __call__(self, cmd, **kwargs):
        self.commands.append(cmd)
        if cmd.startswith("gh pr list"):
            return result(cmd, json.dumps([dict(number=n, headRefName=f"feature/codex-task-{n}") for n in (105, 126)]))
        if cmd.startswith("gh pr view"):
            behind = any(f"gh pr view {n} " in cmd for n in self.behind)
            return result(cmd, json.dumps(dict(mergeStateStatus="BEHIND" if behind else "CLEAN", mergeable=self.mergeable,
                                               headRefOid="unchanged")))
        if cmd.startswith("gh repo view"):
            return result(cmd, "example/repo")
        if cmd.startswith("gh api -X PUT"):
            return result(cmd, "{}")
        if cmd.startswith("gh pr checks"):
            older = "105 " in cmd
            bucket = self.bucket if older else "pass"
            return result(cmd, json.dumps([dict(bucket=bucket, state=bucket.upper(), name="AppleClang",
                                               link="https://github.com/example/repo/actions/runs/900/job/1")]),
                          self.check_code if older and bucket != "pass" else 0)
        if cmd.startswith("gh run view"):
            return result(cmd, json.dumps(dict(status="completed", conclusion="cancelled" if self.bucket == "cancel" else "failure",
                                               jobs=self.jobs)))
        if cmd.startswith("gh run rerun"):
            return result(cmd, code=self.rerun_code, stderr="runner unavailable" if self.rerun_code else "")
        if cmd.startswith("gh pr merge"):
            return result(cmd, "merged")
        raise AssertionError("Unexpected command: " + cmd)


class SupervisorTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.now = 1000
        self.state = SupervisorState(self.directory / "supervisor", 60, lambda: self.now)
        self.stack = contextlib.ExitStack()
        self.addCleanup(self.stack.close)
        self.stack.enter_context(patch.object(supervisor, "STATE", self.state))
        self.stack.enter_context(patch.object(supervisor.time, "time", side_effect=lambda: self.now))
        self.stack.enter_context(contextlib.redirect_stdout(io.StringIO()))

    def reload(self):
        self.state = SupervisorState(self.directory / "supervisor", 60, lambda: self.now)
        supervisor.STATE = self.state
        return self.state

    def queue(self, fake):
        with patch.object(supervisor, "run", side_effect=fake), \
                patch.object(supervisor, "fix_ci", return_value="") as codex:
            supervisor.queue_step()
        return codex

    def test_only_required_checks_gate_the_merge(self):
        def rows(build="pass", precommit="pass"):
            link = "https://github.com/example/repo/actions/runs/7/job/1"
            return [dict(bucket=build, state=build.upper(), name="build", link=link),
                    dict(bucket=precommit, state=precommit.upper(), name="pre-commit", link=link),
                    dict(bucket="pending", state="PENDING", name="MSVC synthetic tests", link=link),
                    dict(bucket="fail", state="FAILURE", name="AppleClang synthetic tests", link=link)]

        def gh(table):
            def call(cmd, **kwargs):
                if cmd.startswith("gh pr checks"):
                    return table
                return dict(status="completed", conclusion="failure",
                            jobs=[dict(conclusion="failure", steps=[dict(status="completed", conclusion="failure")])])
            return call

        with patch.object(supervisor, "run", side_effect=lambda cmd, **kw: result(cmd, "[]")), patch.object(supervisor, "gh_json", side_effect=gh(rows())):
            self.assertEqual(supervisor.ci_state(5), ("passed", []))      # slower platform jobs do not hold the merge
        with patch.object(supervisor, "run", side_effect=lambda cmd, **kw: result(cmd, "[]")), patch.object(supervisor, "gh_json", side_effect=gh(rows(build="pending"))):
            self.assertEqual(supervisor.ci_state(5), ("pending", []))     # a required check still gates it
        with patch.object(supervisor, "run", side_effect=lambda cmd, **kw: result(cmd, "[]")), patch.object(supervisor, "gh_json", side_effect=gh(rows(precommit="fail"))):
            self.assertEqual(supervisor.ci_state(5)[0], "failed")        # and a required failure is a code failure
        with patch.object(supervisor, "REQUIRED_CHECKS", set()), patch.object(supervisor, "run", side_effect=lambda cmd, **kw: result(cmd, "[]")), patch.object(supervisor, "gh_json", side_effect=gh(rows())):
            self.assertEqual(supervisor.ci_state(5)[0], "failed")        # an empty list restores gating on every check

    def test_behind_prs_at_the_head_of_the_queue_are_updated_server_side(self):
        fake = FakeRunner("pass")
        fake.behind = {105}
        codex = self.queue(fake)
        codex.assert_not_called()
        self.assertIn("gh api -X PUT repos/example/repo/pulls/105/update-branch", fake.commands)
        self.assertNotIn("gh pr merge 105 --squash --delete-branch", fake.commands)   # its CI restarts on the new head
        self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)      # a newer green PR is not held back

    def test_checks_fall_back_to_status_check_rollup_when_gh_has_no_json_flag(self):
        rollup = dict(statusCheckRollup=[
            dict(name="build", status="COMPLETED", conclusion="SUCCESS", detailsUrl="https://x/actions/runs/1/job/2"),
            dict(name="pre-commit", status="COMPLETED", conclusion="SUCCESS", detailsUrl="https://x/actions/runs/3/job/4"),
            dict(name="MSVC synthetic tests", status="IN_PROGRESS", conclusion="", detailsUrl="https://x/actions/runs/1/job/5"),
            dict(name="AppleClang synthetic tests", status="COMPLETED", conclusion="CANCELLED", detailsUrl="https://x/actions/runs/1/job/6"),
            dict(name="Compare", status="COMPLETED", conclusion="SKIPPED", detailsUrl="")])

        def run_old_gh(cmd, **kwargs):
            return result(cmd, "", code=1, stderr="unknown flag: --json")

        with patch.object(supervisor, "run", side_effect=run_old_gh), patch.object(supervisor, "gh_json", return_value=rollup):
            checks = supervisor.pr_checks(5)
            self.assertEqual([c["bucket"] for c in checks], ["pass", "pass", "pending", "cancel", "skipping"])
            self.assertEqual(supervisor.ci_state(5), ("passed", []))   # only build and pre-commit gate the merge

    def test_ready_pr_merges_before_a_long_sync_of_an_older_pr(self):
        fake = FakeRunner("pass")
        fake.behind = {105}
        with patch.object(supervisor, "PREWARM_DEPTH", 0), \
                patch.object(supervisor, "sync_with_develop", side_effect=AssertionError("sync must not run first")):
            codex = self.queue(fake)
        codex.assert_not_called()
        self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)

    def test_cancelled_rerun_skips_to_newer_pr_without_codex(self):
        fake = FakeRunner()
        codex = self.queue(fake)
        codex.assert_not_called()
        self.assertIn("gh run rerun 900 --failed", fake.commands)
        self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)
        self.assertNotIn("ci", self.state.record(105)["attempts"])

    def test_runner_not_acquired_failure_is_infrastructure(self):
        for jobs in ([], [{"name": "job was not acquired by Runner", "conclusion": "failure", "steps": []}]):
            fake = FakeRunner("fail", jobs)
            self.queue(fake).assert_not_called()
            self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)

    def test_cancelled_check_without_run_link_is_deferred_without_code_fix(self):
        fake = FakeRunner()

        def command(cmd, **kwargs):
            if cmd.startswith("gh pr checks 105"):
                return result(cmd, json.dumps([dict(bucket="cancel", link="", name="external check")]), code=1)
            return fake(cmd, **kwargs)

        self.queue(command).assert_not_called()
        self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)

    def test_actual_step_failure_uses_code_budget(self):
        fake = FakeRunner("fail", [dict(conclusion="failure", steps=[dict(status="completed", conclusion="failure")])])
        codex = self.queue(fake)
        codex.assert_called_once_with(105, "feature/codex-task-105")
        self.assertEqual(self.state.record(105)["attempts"], {"ci": 1})
        self.assertFalse(any("rerun" in cmd for cmd in fake.commands))

    def test_reruns_are_delayed_bounded_and_persisted(self):
        fake = FakeRunner()
        fake.rerun_code = 1
        for count in range(supervisor.INFRA_RETRIES):
            self.queue(fake)
            self.reload()
            self.queue(fake)  # The same failed check cannot bypass the saved delay.
            self.assertEqual(fake.commands.count("gh run rerun 900 --failed"), count + 1)
            self.now += supervisor.RETRY_DELAY
        self.queue(fake)
        self.assertTrue(self.state.blocked(105))
        self.assertEqual(fake.commands.count("gh run rerun 900 --failed"), supervisor.INFRA_RETRIES)
        self.assertNotIn("ci", self.state.record(105)["attempts"])
        self.now += 61
        self.queue(fake)
        self.assertEqual(fake.commands.count("gh run rerun 900 --failed"), supervisor.INFRA_RETRIES + 1)

    def test_pending_does_not_block_newer_pr(self):
        fake = FakeRunner("pending")
        fake.check_code = 8
        self.queue(fake)
        self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)

    def test_cancelled_with_executed_steps_still_never_uses_codex(self):
        fake = FakeRunner("cancel", [dict(conclusion="cancelled", steps=[dict(status="completed", conclusion="success")])])
        self.queue(fake).assert_not_called()
        self.assertIn("gh run rerun 900 --failed", fake.commands)

    def test_no_steps_other_than_skips_is_infrastructure(self):
        fake = FakeRunner("fail", [dict(conclusion="failure", steps=[dict(status="completed", conclusion="skipped")])])
        self.queue(fake).assert_not_called()
        self.assertIn("gh run rerun 900 --failed", fake.commands)

    def test_accepted_rerun_waits_across_restart(self):
        fake = FakeRunner()
        self.queue(fake)
        self.reload()
        self.queue(fake)
        self.assertEqual(fake.commands.count("gh run rerun 900 --failed"), 1)
        self.assertEqual(fake.commands.count("gh pr merge 126 --squash --delete-branch"), 2)

    def test_running_workflow_is_pending_even_if_old_check_is_cancelled(self):
        fake = FakeRunner()

        def command(cmd, **kwargs):
            if cmd.startswith("gh run view"):
                return result(cmd, json.dumps(dict(status="queued", jobs=[])))
            return fake(cmd, **kwargs)

        self.queue(command).assert_not_called()
        self.assertNotIn("gh run rerun 900 --failed", fake.commands)
        self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)

    def test_exhausted_code_budget_skips_oldest_pr(self):
        fake = FakeRunner("fail", [dict(conclusion="failure", steps=[dict(status="completed", conclusion="failure")])])
        for _ in range(supervisor.MAX_ATTEMPTS):
            self.queue(fake)
            self.reload()
        self.queue(fake).assert_not_called()
        self.assertTrue(self.state.blocked(105))
        self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)

    def test_unknown_mergeability_skips_only_affected_pr(self):
        fake = FakeRunner("pass")

        def command(cmd, **kwargs):
            if cmd.startswith("gh pr view 105"):
                return result(cmd, json.dumps(dict(mergeStateStatus="UNKNOWN", mergeable="UNKNOWN", headRefOid="same")))
            return fake(cmd, **kwargs)

        self.queue(command).assert_not_called()
        self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)

    def test_stuck_persists_expires_and_does_not_block_newer_pr(self):
        self.state.block(105, "synthetic conflict failure")
        self.reload()
        fake = FakeRunner()
        self.queue(fake)
        self.assertFalse(any(cmd.startswith("gh pr checks 105") for cmd in fake.commands))
        self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)
        self.now += 61
        self.assertFalse(self.state.blocked(105))
        self.assertTrue(self.state.begin(105, "sync", 3))

    def test_interrupted_attempt_keeps_budget_and_outcome(self):
        self.assertTrue(self.state.begin(105, "sync", 3))
        self.reload().recover()
        self.assertEqual(self.state.record(105)["attempts"], {"sync": 1})
        self.assertIn("interrupted", self.state.record(105)["last"])
        self.assertNotIn("active", self.state.record(105))
        self.state.begin(105, "sync", 3)
        self.state.finish(105, "sync", "build failed")
        self.reload()
        self.assertEqual(self.state.record(105)["attempts"]["sync"], 2)
        events = [json.loads(line) for line in (self.directory / "supervisor/attempts.jsonl").read_text().splitlines()]
        self.assertEqual([event["outcome"] for event in events],
                         ["started", "interrupted before completion", "started", "build failed"])

    def test_successful_head_change_renews_budget_but_same_head_does_not(self):
        self.state.observe_head(105, "old")
        for _ in range(3):
            self.assertTrue(self.state.begin(105, "ci", 3))
            self.state.finish(105, "ci", "no changes")
        self.reload().observe_head(105, "old")
        self.assertFalse(self.state.begin(105, "ci", 3))
        self.state.observe_head(105, "new")
        self.assertTrue(self.state.begin(105, "ci", 3))

    def test_api_failure_skips_pr_and_never_merges_unchecked_pr(self):
        fake = FakeRunner()

        def command(cmd, **kwargs):
            if cmd.startswith("gh pr checks 105"):
                return result(cmd, code=4, stderr="authentication failed")
            return fake(cmd, **kwargs)

        self.queue(command).assert_not_called()
        self.assertNotIn("gh pr merge 105 --squash --delete-branch", fake.commands)
        self.assertIn("gh pr merge 126 --squash --delete-branch", fake.commands)

    def test_fix_exception_is_logged_and_budget_survives(self):
        fake = FakeRunner("fail", [dict(conclusion="failure", steps=[dict(status="completed", conclusion="failure")])])
        with patch.object(supervisor, "run", side_effect=fake), \
                patch.object(supervisor, "fix_ci", side_effect=TimeoutError("Codex killed")):
            supervisor.queue_step()
        self.reload()
        self.assertEqual(self.state.record(105)["attempts"], {"ci": 1})
        self.assertEqual(self.state.record(105)["last"], "Codex killed")

    def test_sync_interruption_records_outcome_and_recovers_next_pass(self):
        fake = FakeRunner("pass")
        fake.mergeable = "CONFLICTING"
        commands = []

        def land_command(cmd):
            commands.append(cmd)
            if cmd == "git diff --name-only --diff-filter=U":
                return result(cmd, "source.cpp")
            if cmd == "git status --porcelain":
                return result(cmd, "UU source.cpp")
            return result(cmd)

        with patch.object(supervisor, "run", side_effect=fake), \
                patch.object(supervisor.os.path, "isdir", return_value=True), \
                patch.object(supervisor, "lrun", side_effect=land_command), \
                patch.object(supervisor, "codex_in_land", side_effect=TimeoutError("session killed")) as codex:
            supervisor.queue_step()
            codex.assert_called_once()
        self.reload().recover()
        self.assertEqual(self.state.record(105)["attempts"], {"sync": 1})
        self.assertEqual(self.state.record(105)["last"], "session killed")
        self.assertTrue(any("merge --abort" in cmd for cmd in commands))

    def test_status_includes_pr_budget_ci_and_reason(self):
        self.state.begin(105, "ci", 3)
        self.state.finish(105, "ci", "compile failed")
        self.state.block(105, "compile failed")
        fake = FakeRunner()

        def command(cmd, **kwargs):
            if cmd.endswith(" status"):
                return result(cmd, "task-105 harvested session=example\n")
            return fake(cmd, **kwargs)

        out = io.StringIO()
        with patch.object(supervisor, "run", side_effect=command), \
                patch.object(orch, "names", return_value=["task-105"]), \
                patch.object(orch, "load", return_value=dict(name="task-105", status="harvested", harvested=True)), \
                patch.object(orch, "refresh", side_effect=lambda task: task), contextlib.redirect_stdout(out):
            supervisor.report_status()
        self.assertIn("task-105 harvested", out.getvalue())
        self.assertIn('attempts={"ci": 1} STUCK=compile failed', out.getvalue())
        self.assertIn("CI=infrastructure", out.getvalue())

    def test_task_feedback_budget_persists(self):
        with patch.object(supervisor, "run", return_value=result("feedback", "resumed")) as runner:
            for _ in range(4):
                supervisor.return_to_codex("task", "retry landing")
                self.reload()
        self.assertEqual(runner.call_count, 3)
        self.assertTrue(self.state.blocked("task:task"))

    def test_land_recovers_interrupted_merge_before_checkout(self):
        calls = []

        def command(cmd):
            calls.append(cmd)
            return result(cmd, "UU file.cpp\n" if cmd == "git status --porcelain" else "")

        with patch.object(supervisor.os.path, "isdir", return_value=True), \
                patch.object(supervisor, "lrun", side_effect=command):
            supervisor.ensure_land()
        self.assertLess(calls.index("git merge --abort"), calls.index("git reset -q --hard"))
        self.assertEqual(calls[-1], "git checkout -q --detach origin/develop")

    def test_land_recovery_failure_stops_work(self):
        with patch.object(supervisor.os.path, "isdir", return_value=True), \
                patch.object(supervisor, "lrun", return_value=result("git", code=1)):
            # No dirty state: there is nothing to recover.
            supervisor.ensure_land()
        with patch.object(supervisor.os.path, "isdir", return_value=True), \
                patch.object(supervisor, "lrun", side_effect=[result("merge"), result("status", "M file"),
                                                             result("abort"), result("reset", code=1)]):
            with self.assertRaisesRegex(RuntimeError, "LAND recovery failed"):
                supervisor.ensure_land()

    def test_orch_foreign_and_missing_process_sessions_are_unknown(self):
        with patch.object(orch, "ROOT", str(self.directory)), patch.object(orch, "save"), \
                patch.object(orch, "sh", return_value=result("ps", "")):
            for task in (dict(name="task", session="old"),
                         dict(name="task", session="old", host="another-machine", status="stopped")):
                self.assertEqual(orch.refresh(task)["status"], "unknown")
            self.assertEqual(orch.refresh(dict(name="task", harvested=True))["status"], "harvested")

    def test_orch_local_process_is_running(self):
        with patch.object(orch, "ROOT", str(self.directory)), patch.object(orch, "save"), \
                patch.object(orch, "sh", return_value=result("ps", "/usr/bin/codex exec resume session-123 -o output.last\n")):
            self.assertEqual(orch.refresh(dict(name="task", session="session-123"))["status"], "running")


if __name__ == "__main__":
    unittest.main()
