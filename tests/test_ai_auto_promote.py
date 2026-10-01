"""Decision checks for the post-training automatic promotion gate."""

from __future__ import annotations

import json
import sys
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import ai_auto_promote as gate  # noqa: E402


def fixtures(role: str = "guard") -> dict[str, dict[str, float]]:
    scenarios = ("open", "center_cover", "elevated", "group")
    if role == "robot":
        scenarios += ("boss_cover",)
    result = {}
    for frames in (360, 720):
        for scenario in scenarios:
            result[f"map{frames}:{scenario}"] = {
                "damage": 100.0, "travel": 20.0, "closest": 30.0,
                "defeats": 0.0, "player_wins": 0.0,
                "neural_ticks": 100.0}
    for scenario in ("ground_strafe", "bhop"):
        result[f"duel:{scenario}"] = {
            "damage": 100.0, "travel": 20.0, "closest": 0.0,
            "defeats": 0.0, "player_wins": 0.0,
            "neural_ticks": 100.0}
    return result


class PromotionDecisionTests(unittest.TestCase):
    def test_selection_and_final_seeds_are_disjoint(self) -> None:
        self.assertFalse(set(gate.QUICK_SEED_POOL)
                         & set(gate.VALIDATION_SEED_POOL))
        self.assertFalse(set(gate.QUICK_SEED_POOL)
                         & set(gate.FINAL_SEED_POOL))
        self.assertFalse(set(gate.VALIDATION_SEED_POOL)
                         & set(gate.FINAL_SEED_POOL))

    def test_meaningful_combat_gain_passes(self) -> None:
        active = fixtures()
        candidate = fixtures()
        for scenario in ("open", "center_cover", "elevated", "group"):
            candidate[f"map360:{scenario}"]["damage"] = 106.0
        self.assertTrue(gate.decide("guard", active, candidate, 15)[0])

    def test_cover_regression_blocks_even_if_total_improves(self) -> None:
        active = fixtures()
        candidate = fixtures()
        candidate["map720:center_cover"]["damage"] = 80.0
        candidate["map360:open"]["damage"] = 150.0
        passed, _, reason = gate.decide("guard", active, candidate, 15)
        self.assertFalse(passed)
        self.assertIn("center_cover", reason)

    def test_bhop_regression_blocks(self) -> None:
        active = fixtures()
        candidate = fixtures()
        candidate["duel:bhop"]["damage"] = 85.0
        self.assertFalse(gate.decide("guard", active, candidate, 15)[0])

    def test_more_player_wins_block_promotion(self) -> None:
        active = fixtures()
        candidate = fixtures()
        candidate["map360:open"]["damage"] = 130.0
        candidate["duel:bhop"]["player_wins"] = 3.0
        self.assertFalse(gate.decide("guard", active, candidate, 15)[0])

    def test_boss_lethality_spike_blocks(self) -> None:
        active = fixtures("robot")
        candidate = fixtures("robot")
        candidate["map720:boss_cover"]["defeats"] = 3.0
        candidate["map360:open"]["damage"] = 125.0
        passed, _, reason = gate.decide("robot", active, candidate, 15)
        self.assertFalse(passed)
        self.assertIn("lethality", reason)

    def test_mobility_regression_blocks(self) -> None:
        active = fixtures()
        candidate = fixtures()
        for key, value in candidate.items():
            if key.startswith("map360:"):
                value["travel"] = 10.0
                value["damage"] = 110.0
        self.assertFalse(gate.decide("guard", active, candidate, 15)[0])

    def test_no_material_improvement_does_not_replace_active(self) -> None:
        active = fixtures()
        candidate = fixtures()
        self.assertFalse(gate.decide("guard", active, candidate, 15)[0])

    def test_no_map_damage_never_promotes(self) -> None:
        active = fixtures()
        candidate = fixtures()
        for key in candidate:
            if key.startswith("map360:"):
                candidate[key]["damage"] = 0.0
        self.assertFalse(gate.decide("guard", active, candidate, 15,
                                     quick=True)[0])

    def test_duplicate_weights_keep_latest_checkpoint(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            run_dir = Path(directory)
            for generation in (1, 2):
                (run_dir / f"guard_generation_{generation:06d}.brain").write_text(
                    f"generation {generation}\nweights 2\n42\n84\n", encoding="ascii")
            selected = gate.discover(run_dir)
            self.assertEqual(1, len(selected["guard"]))
            self.assertEqual("guard_generation_000002.brain",
                             selected["guard"][0].name)


class PromotionWriteTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.source = self.root / "assets/ai/worker.brain"
        self.runtime = self.root / "build/release/ai/worker.brain"
        self.candidate = self.root / "candidate.brain"
        self.source.parent.mkdir(parents=True)
        self.runtime.parent.mkdir(parents=True)
        self.source.write_bytes(b"old brain")
        self.runtime.write_bytes(b"old brain")
        self.candidate.write_bytes(b"new brain")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_promotion_updates_both_copies_and_receipt(self) -> None:
        clean = subprocess.CompletedProcess([], 0, stdout="", stderr="")
        with mock.patch.object(gate, "ROOT", self.root), \
                mock.patch.object(gate.subprocess, "run", return_value=clean), \
                mock.patch.object(gate, "evaluator_rows"):
            gate.promote("worker", self.candidate, gate.sha256(self.source),
                         self.source.parent, self.runtime.parent,
                         self.root / "build/release/gdp_ai_gameplay_eval")
        self.assertEqual(b"new brain", self.source.read_bytes())
        self.assertEqual(b"new brain", self.runtime.read_bytes())
        state = self.root / "build/ai-promotion-state.json"
        self.assertEqual(gate.sha256(self.source),
                         json.loads(state.read_text())["worker"])

    def test_failed_validation_rolls_back_both_copies(self) -> None:
        clean = subprocess.CompletedProcess([], 0, stdout="", stderr="")
        with mock.patch.object(gate, "ROOT", self.root), \
                mock.patch.object(gate.subprocess, "run", return_value=clean), \
                mock.patch.object(gate, "evaluator_rows",
                                  side_effect=RuntimeError("invalid brain")):
            with self.assertRaisesRegex(RuntimeError, "invalid brain"):
                gate.promote("worker", self.candidate, gate.sha256(self.source),
                             self.source.parent, self.runtime.parent,
                             self.root / "build/release/gdp_ai_gameplay_eval")
        self.assertEqual(b"old brain", self.source.read_bytes())
        self.assertEqual(b"old brain", self.runtime.read_bytes())

    def test_unrelated_dirty_brain_is_not_overwritten(self) -> None:
        dirty = subprocess.CompletedProcess([], 0,
                                            stdout=" M assets/ai/worker.brain\n",
                                            stderr="")
        with mock.patch.object(gate, "ROOT", self.root), \
                mock.patch.object(gate.subprocess, "run", return_value=dirty):
            with self.assertRaisesRegex(RuntimeError, "unrelated uncommitted"):
                gate.promote("worker", self.candidate, gate.sha256(self.source),
                             self.source.parent, self.runtime.parent,
                             self.root / "build/release/gdp_ai_gameplay_eval")
        self.assertEqual(b"old brain", self.source.read_bytes())


class PromotionWorkflowTests(unittest.TestCase):
    def test_final_holdout_failure_cannot_promote(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidate = root / "worker_generation_000007.brain"
            candidate.write_text("seed 42\n", encoding="ascii")
            active_dir = root / "assets/ai"
            active_dir.mkdir(parents=True)
            (active_dir / "worker.brain").write_bytes(b"active")
            evaluator = root / "gdp_ai_gameplay_eval"
            evaluator.touch()
            with mock.patch.object(gate, "ROOT", root), \
                    mock.patch.object(gate, "measure", return_value={}), \
                    mock.patch.object(gate, "decide", side_effect=[
                        (True, 1.0, "quick"), (True, 2.0, "validation"),
                        (False, 0.0, "final regression")]), \
                    mock.patch.object(gate, "promote") as promotion:
                gate.run_gate({"worker": [candidate]}, evaluator, active_dir,
                              root / "build/release/ai", False)
            promotion.assert_not_called()
            reports = list((root / "build/ai-promotion-reports").glob("*.json"))
            self.assertEqual(1, len(reports))
            report = json.loads(reports[0].read_text())
            self.assertEqual("final regression", report["final_reason"])
            self.assertIsNone(report["winner"])


if __name__ == "__main__":
    unittest.main()
