#!/usr/bin/env python3
"""Train (or inspect a completed run) and promote only measured AI improvements.

The normal game loads assets/ai/*.brain.  This tool deliberately does not
commit, push, or touch checkpoints; it updates one active role at a time only
after a paired gameplay gate and a headless load check succeed.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import os
import re
import subprocess
import sys
import tempfile
from contextlib import contextmanager
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ROLES = ("worker", "guard", "officer", "engineer", "robot")
QUICK_SEED_POOL = (7, 1001, 424242, 17, 24680)
VALIDATION_SEED_POOL = (12345, 19, 9901, 111222333, 20260930,
                        56789, 314159, 271828, 161803, 31415926,
                        654321, 987654)
FINAL_SEED_POOL = (20261002, 8675309, 1357911, 2468024, 314159265,
                   987654321, 1123581321, 20261231, 7777777, 76543210,
                   19191919, 45454545)
CHECKPOINT = re.compile(r"^(worker|guard|officer|engineer|robot)_generation_(\d+)\.brain$")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def policy_digest(path: Path) -> str:
    """Ignore generation/fitness metadata when deduplicating identical weights."""
    lines = path.read_text(encoding="ascii").splitlines()
    marker = next((i for i, line in enumerate(lines) if line.startswith("weights ")), None)
    if marker is None:
        raise ValueError(f"Missing weights: {path}")
    count = int(lines[marker].split()[1])
    if count <= 0 or len(lines) < marker + 1 + count:
        raise ValueError(f"Truncated weights: {path}")
    return hashlib.sha256("\n".join(lines[marker + 1:marker + 1 + count]).encode()).hexdigest()


def training_seed(path: Path) -> int:
    for line in path.read_text(encoding="ascii").splitlines()[:8]:
        if line.startswith("seed "):
            return int(line.split()[1])
    raise ValueError(f"Missing training seed: {path}")


def discover(run_dir: Path) -> dict[str, list[Path]]:
    if not run_dir.is_dir():
        raise ValueError(f"Training directory does not exist: {run_dir}")
    newest: dict[str, dict[str, tuple[int, Path]]] = {role: {} for role in ROLES}
    for path in run_dir.rglob("*.brain"):
        match = CHECKPOINT.fullmatch(path.name)
        if not match or not path.is_file():
            continue
        role, generation_text = match.groups()
        generation = int(generation_text)
        digest = policy_digest(path)
        old = newest[role].get(digest)
        if old is None or generation > old[0]:
            newest[role][digest] = (generation, path)
    return {role: [item[1] for item in sorted(policies.values(),
            key=lambda item: item[0], reverse=True)]
            for role, policies in newest.items() if policies}


def evaluator_rows(evaluator: Path, active_dir: Path, role: str,
                   candidate: Path | None, seed: int, frames: int,
                   duel: bool) -> dict[str, dict[str, str]]:
    command = [str(evaluator), "--brain-dir", str(active_dir),
               "--seed", str(seed), "--frames", str(frames)]
    if duel:
        command.append("--duel")
    if candidate is not None:
        command.extend(("--brain", f"{role}={candidate}"))
    result = subprocess.run(command, cwd=ROOT, capture_output=True,
                            text=True, check=False, timeout=120)
    if result.returncode:
        raise RuntimeError(f"Evaluator failed for {role}/{candidate}: {result.stderr.strip()}")
    rows = {}
    for row in csv.DictReader(io.StringIO(result.stdout)):
        if row["policy"] == "neural" and row["archetype"] == role:
            rows[row["scenario"]] = row
    expected = {"ground_strafe", "bhop"} if duel else (
        {"open", "center_cover", "elevated", "group", "boss_cover"}
        if role == "robot" else {"open", "center_cover", "elevated", "group"})
    if set(rows) != expected:
        raise RuntimeError(f"Incomplete evaluator output for {role}: {set(rows)}")
    return rows


def measure(evaluator: Path, active_dir: Path, role: str,
            candidate: Path | None, seeds: tuple[int, ...],
            include_long: bool) -> dict[str, dict[str, float]]:
    totals: dict[str, dict[str, float]] = {}
    for seed in seeds:
        modes = ((360, False), (360, True))
        if include_long:
            modes += ((720, False),)
        for frames, duel in modes:
            rows = evaluator_rows(evaluator, active_dir, role, candidate,
                                  seed, frames, duel)
            for scenario, row in rows.items():
                key = f"{'duel' if duel else f'map{frames}'}:{scenario}"
                item = totals.setdefault(key, {"damage": 0.0, "travel": 0.0,
                                               "closest": 0.0, "defeats": 0.0,
                                               "player_wins": 0.0,
                                               "neural_ticks": 0.0})
                item["damage"] += float(row["damage_to_player"] if duel
                                        else row["player_damage"])
                item["travel"] += float(row["enemy_travel"])
                item["closest"] += float(row.get("closest_distance") or 0)
                item["defeats"] += float(row["outcome"] == "enemy_win") if duel \
                    else float(int(row["defeat_frame"]) > 0)
                if duel:
                    item["player_wins"] += float(row["outcome"] == "player_win")
                item["neural_ticks"] += float(row["neural_ticks"])
    return totals


def ratio(candidate: float, active: float) -> float:
    return candidate / active if active > 0.001 else (1.0 if candidate <= 0.001 else 2.0)


def decide(role: str, active: dict[str, dict[str, float]],
           candidate: dict[str, dict[str, float]], seeds: int,
           quick: bool = False) -> tuple[bool, float, str]:
    if set(active) != set(candidate):
        return False, 0.0, "scenario mismatch"
    maps = [key for key in active if key.startswith("map360:")]
    active_total = sum(active[key]["damage"] for key in maps)
    candidate_total = sum(candidate[key]["damage"] for key in maps)
    bhop = "duel:bhop"
    if candidate_total <= 0.001:
        return False, 0.0, "no map combat damage"
    if ratio(candidate_total, active_total) < (0.85 if quick else 0.98):
        return False, 0.0, "map combat regression"
    if ratio(candidate[bhop]["damage"], active[bhop]["damage"]) \
            < (0.75 if quick else 0.90):
        return False, 0.0, "Bhop combat regression"
    if quick:
        return True, ratio(candidate_total, active_total), "quick screen passed"

    for key in maps + [key for key in active if key.startswith("map720:")]:
        if ratio(candidate[key]["damage"], active[key]["damage"]) < 0.90:
            return False, 0.0, f"combat regression in {key}"
        # A new boss that kills the player much more often in a fixture can
        # distort the campaign even when its damage score looks attractive.
        if role == "robot" and candidate[key]["defeats"] \
                > active[key]["defeats"] + max(1, seeds // 10):
            return False, 0.0, f"boss lethality spike in {key}"
    for key, floor in (("duel:ground_strafe", 0.75), (bhop, 0.90)):
        if ratio(candidate[key]["damage"], active[key]["damage"]) < floor:
            return False, 0.0, f"duel regression in {key}"
        if candidate[key]["player_wins"] \
                > active[key]["player_wins"] + max(1, seeds // 10):
            return False, 0.0, f"duel survival regression in {key}"
    active_travel = sum(active[key]["travel"] for key in maps)
    candidate_travel = sum(candidate[key]["travel"] for key in maps)
    if active_travel > 1.0 and ratio(candidate_travel, active_travel) < 0.80:
        return False, 0.0, "map mobility regression"
    active_ticks = sum(active[key]["neural_ticks"] for key in maps)
    candidate_ticks = sum(candidate[key]["neural_ticks"] for key in maps)
    if active_ticks > 0 and candidate_ticks < active_ticks * 0.65:
        return False, 0.0, "excessive scripted fallback"

    cover = "map720:boss_cover" if role == "robot" else "map720:center_cover"
    cover_gain = ratio(candidate[cover]["damage"], active[cover]["damage"])
    active_closest = active[cover]["closest"]
    candidate_closest = candidate[cover]["closest"]
    navigation_gain = (active_closest > 0 and candidate_closest < active_closest * 0.85
                       and candidate[cover]["damage"] >= active[cover]["damage"])
    meaningful_gain = (ratio(candidate_total, active_total) >= 1.05
                       or cover_gain >= 1.15
                       or navigation_gain
                       or ratio(candidate[bhop]["damage"], active[bhop]["damage"]) >= 1.15)
    if not meaningful_gain:
        return False, 0.0, "no meaningful gameplay improvement"
    score = (ratio(candidate_total, active_total)
             + 0.35 * min(cover_gain, 2.0)
             + 0.20 * min(ratio(candidate[bhop]["damage"], active[bhop]["damage"]), 2.0)
             + 0.10 * min(ratio(candidate_travel, active_travel), 2.0))
    return True, score, "gameplay gate passed"


@contextmanager
def promotion_lock():
    lock_path = ROOT / "build" / "ai-promotion.lock"
    lock_path.parent.mkdir(parents=True, exist_ok=True)
    with lock_path.open("a+b") as lock:
        lock.seek(0, os.SEEK_END)
        if lock.tell() == 0:
            lock.write(b"\0")
            lock.flush()
        lock.seek(0)
        if os.name == "nt":
            import msvcrt
            msvcrt.locking(lock.fileno(), msvcrt.LK_LOCK, 1)
            try:
                yield
            finally:
                lock.seek(0)
                msvcrt.locking(lock.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl
            fcntl.flock(lock.fileno(), fcntl.LOCK_EX)
            try:
                yield
            finally:
                fcntl.flock(lock.fileno(), fcntl.LOCK_UN)


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix=f".{path.name}.",
                                     delete=False) as temporary:
        temporary.write(data)
        temporary_name = Path(temporary.name)
    try:
        os.replace(temporary_name, path)
    finally:
        temporary_name.unlink(missing_ok=True)


def promote(role: str, candidate: Path, expected_active_hash: str,
            active_dir: Path, runtime_dir: Path, evaluator: Path) -> None:
    source = active_dir / f"{role}.brain"
    runtime = runtime_dir / f"{role}.brain"
    state_path = ROOT / "build" / "ai-promotion-state.json"
    with promotion_lock():
        if sha256(source) != expected_active_hash:
            raise RuntimeError(f"Active {role} changed during evaluation; retry the gate")
        state = json.loads(state_path.read_text()) if state_path.exists() else {}
        status = subprocess.run(["git", "status", "--porcelain", "--",
                                 str(source.relative_to(ROOT))], cwd=ROOT,
                                capture_output=True, text=True, check=True)
        if status.stdout.strip() and state.get(role) != expected_active_hash:
            raise RuntimeError(f"Active {role} has unrelated uncommitted changes")
        if not runtime.is_file() or sha256(runtime) != expected_active_hash:
            raise RuntimeError(f"Runtime {role} differs from active asset; rebuild first")
        previous = source.read_bytes()
        previous_state = state_path.read_bytes() if state_path.exists() else None
        replacement = candidate.read_bytes()
        try:
            atomic_write(source, replacement)
            atomic_write(runtime, replacement)
            evaluator_rows(evaluator, active_dir, role, None, 12345, 360, False)
            game = evaluator.parent / ("gdp.exe" if os.name == "nt" else "gdp")
            smoke = subprocess.run([str(game), "--headless", "--frames", "2",
                                    "--seed", "12345", "--test-brain-dir",
                                    str(active_dir)], cwd=ROOT,
                                   capture_output=True, text=True,
                                   check=False, timeout=30)
            if smoke.returncode:
                raise RuntimeError(f"Promoted brain failed headless load: {smoke.stderr}")
            state[role] = sha256(source)
            atomic_write(state_path, (json.dumps(state, indent=2) + "\n").encode())
        except Exception:
            atomic_write(source, previous)
            atomic_write(runtime, previous)
            if previous_state is None:
                state_path.unlink(missing_ok=True)
            else:
                atomic_write(state_path, previous_state)
            raise


def run_gate(groups: dict[str, list[Path]], evaluator: Path,
             active_dir: Path, runtime_dir: Path, dry_run: bool) -> int:
    if not evaluator.is_file():
        raise ValueError(f"Build the evaluator first: {evaluator}")
    reports = ROOT / "build" / "ai-promotion-reports"
    reports.mkdir(parents=True, exist_ok=True)
    for role, candidates in groups.items():
        candidates = [path for path in candidates
                      if int(CHECKPOINT.fullmatch(path.name).group(2)) >= 7]
        if not candidates:
            print(f"AI_PROMOTION role={role} decision=KEEP "
                  "reason=incomplete_curriculum", flush=True)
            continue
        current_hash = sha256(active_dir / f"{role}.brain")
        run_seeds = {training_seed(candidate) for candidate in candidates}
        if len(run_seeds) != 1:
            raise ValueError(f"Mixed training seeds for {role}: {run_seeds}")
        run_seed = next(iter(run_seeds))
        quick_seeds = tuple(seed for seed in QUICK_SEED_POOL
                            if seed != run_seed)[:3]
        validation_seeds = tuple(seed for seed in VALIDATION_SEED_POOL
                                 if seed != run_seed)[:10]
        final_seeds = tuple(seed for seed in FINAL_SEED_POOL
                            if seed != run_seed)[:10]
        quick_baseline = measure(evaluator, active_dir, role, None,
                                 quick_seeds, False)
        validation_baseline = None
        winner: tuple[float, Path] | None = None
        winner_metrics = None
        decisions = []
        for index, candidate in enumerate(candidates, 1):
            quick_result = measure(evaluator, active_dir, role, candidate,
                                   quick_seeds, False)
            passed, score, reason = decide(role, quick_baseline, quick_result,
                                            len(quick_seeds), quick=True)
            if passed:
                if validation_baseline is None:
                    validation_baseline = measure(evaluator, active_dir, role,
                                                  None, validation_seeds, True)
                validation_result = measure(evaluator, active_dir, role,
                                            candidate, validation_seeds, True)
                passed, score, reason = decide(role, validation_baseline,
                                                validation_result,
                                                len(validation_seeds))
            decisions.append({"candidate": str(candidate), "passed": passed,
                              "score": round(score, 5), "reason": reason})
            if passed and (winner is None or score > winner[0]):
                winner = score, candidate
                winner_metrics = validation_result
            if index % 25 == 0:
                print(f"AI_PROMOTION_PROGRESS role={role} checked={index}/{len(candidates)}",
                      flush=True)
        final_baseline = None
        final_result = None
        final_reason = "no validation winner"
        selected_for_final = str(winner[1]) if winner else None
        if winner:
            final_baseline = measure(evaluator, active_dir, role, None,
                                     final_seeds, True)
            final_result = measure(evaluator, active_dir, role, winner[1],
                                   final_seeds, True)
            final_passed, _, final_reason = decide(role, final_baseline,
                                                    final_result,
                                                    len(final_seeds))
            if not final_passed:
                winner = None
        report = {"role": role, "active_sha256": current_hash,
                  "training_seed": run_seed, "quick_seeds": quick_seeds,
                  "validation_seeds": validation_seeds,
                  "final_seeds": final_seeds,
                  "policies_checked": len(candidates), "decisions": decisions,
                  "selected_for_final": selected_for_final,
                  "winner": str(winner[1]) if winner else None,
                  "validation_baseline_metrics": validation_baseline,
                  "validation_winner_metrics": winner_metrics,
                  "final_baseline_metrics": final_baseline,
                  "final_winner_metrics": final_result,
                  "final_reason": final_reason,
                  "dry_run": dry_run}
        # Checkpoint paths can be reused; the timestamp is only a report label,
        # never an input to the deterministic gate.
        from datetime import datetime, timezone
        label = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        report_path = reports / f"{label}-{role}.json"
        atomic_write(report_path, (json.dumps(report, indent=2) + "\n").encode())
        if winner and not dry_run:
            promote(role, winner[1], current_hash, active_dir, runtime_dir, evaluator)
            print(f"AI_PROMOTION role={role} decision=PROMOTE brain={winner[1]} "
                  f"report={report_path}", flush=True)
        else:
            print(f"AI_PROMOTION role={role} decision={'DRY_RUN' if winner else 'KEEP'} "
                  f"brain={winner[1] if winner else '-'} "
                  f"reason={final_reason.replace(' ', '_')} report={report_path}",
                  flush=True)
    return 0


def training_value(command: list[str], option: str) -> str:
    if option not in command or command.index(option) + 1 >= len(command):
        raise ValueError(f"Training command needs {option}")
    return command[command.index(option) + 1]


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("train", "promote"))
    parser.add_argument("--run-dir", type=Path)
    parser.add_argument("--evaluator", type=Path,
                        default=Path("build/release/gdp_ai_gameplay_eval"))
    parser.add_argument("--active-dir", type=Path, default=Path("assets/ai"))
    parser.add_argument("--runtime-dir", type=Path, default=Path("build/release/ai"))
    parser.add_argument("--dry-run", action="store_true")
    if "--" in argv:
        split = argv.index("--")
        options = parser.parse_args(argv[:split])
        command = argv[split + 1:]
    else:
        options = parser.parse_args(argv)
        command = []
    try:
        if options.mode == "train":
            if not command or "--ai-training" not in command:
                raise ValueError("train needs: -- EXECUTABLE --ai-training TYPE ...")
            role = training_value(command, "--ai-training")
            if role not in (*ROLES, "all"):
                raise ValueError(f"Unknown training role: {role}")
            run_dir = Path(training_value(command, "--save-dir"))
            if options.run_dir:
                raise ValueError("--run-dir is only for promote mode")
            result = subprocess.run(command, cwd=ROOT, check=False)
            if result.returncode:
                print(f"AI_PROMOTION decision=SKIP training_exit={result.returncode}",
                      file=sys.stderr)
                return result.returncode
        else:
            if command or options.run_dir is None:
                raise ValueError("promote needs --run-dir and no training command")
            run_dir = options.run_dir
            role = "all"
        def absolute(path: Path) -> Path:
            return path if path.is_absolute() else ROOT / path
        groups = discover(absolute(run_dir))
        if role != "all":
            groups = {role: groups[role]} if role in groups else {}
        if not groups or (role == "all" and options.mode == "train"
                          and len(groups) != len(ROLES)):
            raise ValueError(f"No complete checkpoint set in {run_dir}")
        return run_gate(groups, absolute(options.evaluator),
                        absolute(options.active_dir),
                        absolute(options.runtime_dir), options.dry_run)
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"AI_PROMOTION decision=ERROR reason={error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
