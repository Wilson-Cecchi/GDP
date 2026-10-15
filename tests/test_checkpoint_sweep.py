"""Read-only checkpoint sweep must not mix revisions or active batches."""

import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/zimaos"))
import checkpoint_sweep as sweep


REVISION = "a" * 40


def fixture(root: Path) -> None:
    for name, content in (("compiled/gdp", b"train"),
                          ("compiled/gdp_ai_gameplay_eval", b"evaluate")):
        file = root / name
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_bytes(content)
    (root / "compiled-sha256.txt").write_text("".join(
        f"{hashlib.sha256((root / name).read_bytes()).hexdigest()}  {name}\n"
        for name in ("compiled/gdp", "compiled/gdp_ai_gameplay_eval")))
    data = root / "data"
    data.mkdir()
    (data / "state.json").write_text(json.dumps({
        "protocol": 2, "cooperative": True, "source_commit": REVISION}))
    (data / "status.json").write_text(json.dumps({
        "state": "running", "role": "officer", "phase": "training",
        "log": "/data/logs/officer/batch-live.log"}))


def batch(root: Path, role: str, name: str, generations: range,
          evaluated: bool = False) -> None:
    data = root / "data"
    run = data / "runs" / role / name
    run.mkdir(parents=True)
    for generation in generations:
        brain = run / f"{role}_generation_{generation:06d}.brain"
        brain.write_bytes(f"{role}-{generation}".encode())
        brain.with_suffix(".population").mkdir()
    manifest = data / "manifests" / role / f"{name}.json"
    manifest.parent.mkdir(parents=True)
    manifest.write_text(json.dumps({"source_commit": REVISION,
        "command": ["gdp", "--ai-training", role, "--population", "8",
                    "--cooperative-brain", "--all-scenarios", "--diverse-maps"]}))
    if evaluated:
        report = data / "evaluations" / role / name / "quick" / "report.json"
        report.parent.mkdir(parents=True)
        report.write_text(json.dumps({"role": role,
            "generation": generations[-1],
            "protocol": {"source_revision": REVISION}}))


class CheckpointSweepTests(unittest.TestCase):
    def test_sample_is_deterministic_and_includes_batch_endpoint(self):
        self.assertEqual([(24, "interior"), (47, "interior"), (70, "endpoint")],
                         sweep.choose_generations(list(range(1, 71)), True))
        self.assertEqual([(35, "interior"), (70, "endpoint")],
                         sweep.choose_generations(list(range(1, 71)), False))

    def test_inventory_records_hash_and_skips_active_batch(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            batch(root, "guard", "batch-done", range(1, 5), evaluated=True)
            batch(root, "officer", "batch-live", range(1, 5))
            with patch.object(sweep.service, "valid_checkpoint", return_value=True), \
                 patch.object(sweep.service, "population_metadata", return_value={"population": 8}):
                result = sweep.inventory(root)
            self.assertEqual(["guard"], sorted(set(
                row["role"] for row in result["selections"])))
            endpoint = next(row for row in result["selections"]
                            if row["generation"] == 4)
            self.assertTrue(endpoint["already_evaluated"])
            self.assertEqual(hashlib.sha256(b"guard-4").hexdigest(),
                             endpoint["sha256"])
            self.assertEqual("active", next(row["state"] for row in result["batches"]
                                            if row["role"] == "officer"))

    def test_mismatched_revision_or_binary_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            batch(root, "guard", "batch-wrong", range(1, 3))
            manifest = root / "data/manifests/guard/batch-wrong.json"
            content = json.loads(manifest.read_text())
            content["source_commit"] = "b" * 40
            manifest.write_text(json.dumps(content))
            self.assertEqual([], sweep.inventory(root)["selections"])
            (root / "compiled/gdp").write_bytes(b"tampered")
            with self.assertRaisesRegex(ValueError, "binary hash mismatch"):
                sweep.inventory(root)

    def test_incomplete_population_is_not_selected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            fixture(root)
            batch(root, "robot", "batch-done", range(1, 5), evaluated=True)
            with patch.object(sweep.service, "valid_checkpoint", return_value=True), \
                 patch.object(sweep.service, "population_metadata",
                              side_effect=lambda path, *_: None if "000003" in str(path)
                              else {"population": 8}):
                result = sweep.inventory(root)
            self.assertEqual({2, 4}, {row["generation"]
                                      for row in result["selections"]})
            self.assertEqual(2, result["batches"][0]["valid_selected"])


if __name__ == "__main__":
    unittest.main()
