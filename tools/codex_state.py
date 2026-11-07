"""Crash-safe local supervisor budgets. No repository metadata or network access."""
import json
import os
from pathlib import Path
import threading
import time


class SupervisorState:
    def __init__(self, directory, stuck_seconds=21600, clock=time.time):
        self.directory = Path(directory)
        self.path = self.directory / "state.json"
        self.clock = clock
        self.stuck_seconds = stuck_seconds
        self.lock = threading.RLock()
        self.data = json.loads(self.path.read_text()) if self.path.exists() else {}

    def save(self):
        with self.lock:
            self.directory.mkdir(parents=True, exist_ok=True)
            temporary = self.path.with_suffix(".tmp")
            with temporary.open("w") as stream:
                json.dump(self.data, stream, indent=2)
                stream.flush()
                os.fsync(stream.fileno())
            os.replace(temporary, self.path)
            directory_fd = os.open(self.directory, os.O_RDONLY)
            try:
                os.fsync(directory_fd)
            finally:
                os.close(directory_fd)

    def record(self, key):
        with self.lock:
            return self.data.setdefault(str(key), {"attempts": {}, "runs": {}})

    def event(self, key, kind, count, outcome):
        self.directory.mkdir(parents=True, exist_ok=True)
        row = dict(time=self.clock(), key=str(key), kind=kind, attempt=count, outcome=outcome)
        with (self.directory / "attempts.jsonl").open("a") as stream:
            stream.write(json.dumps(row) + "\n")
            stream.flush()
            os.fsync(stream.fileno())
        print("supervisor attempt", json.dumps(row), flush=True)

    def recover(self):
        """Interrupted work still consumes its recorded attempt, never a fresh budget."""
        with self.lock:
            for key, record in self.data.items():
                if record.get("active"):
                    kind = record.pop("active")
                    record["last"] = "interrupted before completion"
                    self.event(key, kind, record["attempts"][kind], record["last"])
            self.save()

    def blocked(self, key):
        with self.lock:
            record = self.record(key)
            stuck = record.get("stuck")
            if stuck and self.clock() >= stuck["until"]:
                record.update(attempts={}, runs={})
                del record["stuck"]
                self.event(key, "cooldown", 0, "STUCK expired; retry budget renewed")
                self.save()
                return False
            return bool(stuck)

    def block(self, key, reason):
        with self.lock:
            self.record(key)["stuck"] = dict(reason=reason, until=self.clock() + self.stuck_seconds)
            self.save()
            self.event(key, "stuck", 0, reason)

    def begin(self, key, kind, maximum):
        with self.lock:
            if self.blocked(key):
                return False
            record = self.record(key)
            count = record["attempts"].get(kind, 0)
            if count >= maximum:
                self.block(key, f"{kind} exhausted after {count} attempts: {record.get('last', '')}")
                return False
            record["attempts"][kind] = count + 1
            record["active"] = kind
            self.save()
            self.event(key, kind, count + 1, "started")
            return True

    def finish(self, key, kind, outcome):
        with self.lock:
            record = self.record(key)
            record.pop("active", None)
            record["last"] = outcome or "success"
            self.save()
            self.event(key, kind, record["attempts"][kind], record["last"])

    def observe_head(self, key, oid):
        with self.lock:
            record = self.record(key)
            if record.get("head") and record["head"] != oid:
                record.update(attempts={}, runs={})
                record.pop("stuck", None)
            record["head"] = oid
            self.save()
