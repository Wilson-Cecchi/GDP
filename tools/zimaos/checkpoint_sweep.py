#!/usr/bin/env python3
"""Inventory protocol-2 checkpoints and pick a small, reproducible sweep.

This script only reads an existing deployment. It never evaluates a brain,
changes the service, or reads the reserved final evaluation seeds.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re

_previous_environment = {name: os.environ.get(name) for name in
                         ("GDP_COOPERATIVE", "GDP_TRAIN_PROTOCOL")}
os.environ["GDP_COOPERATIVE"] = "1"
os.environ["GDP_TRAIN_PROTOCOL"] = "2"
import train_service as service  # noqa: E402 - select the contract before import
for _name, _value in _previous_environment.items():
    if _value is None:
        os.environ.pop(_name, None)
    else:
        os.environ[_name] = _value


ROLES = ("worker", "guard", "officer", "engineer", "robot")
PRIORITY_ROLES = frozenset(("guard", "robot"))
HASH = re.compile(r"[0-9a-f]{64}")


def _binary_hashes(root: Path) -> dict[str, str]:
    result = {}
    for line in (root / "compiled-sha256.txt").read_text().splitlines():
        digest, name = line.split(maxsplit=1)
        if not HASH.fullmatch(digest):
            raise ValueError(f"invalid binary hash for {name}")
        result[name] = digest
    for name in ("compiled/gdp", "compiled/gdp_ai_gameplay_eval"):
        if name not in result:
            raise ValueError(f"missing binary hash for {name}")
        with (root / name).open("rb") as binary:
            actual = hashlib.file_digest(binary, "sha256").hexdigest()
        if actual != result[name]:
            raise ValueError(f"binary hash mismatch for {name}")
    return result


def choose_generations(generations: list[int], priority: bool) -> list[tuple[int, str]]:
    """Choose interior generations plus the already-evaluated batch endpoint."""
    if not generations:
        return []
    ordered = sorted(set(generations))
    fractions = (1 / 3, 2 / 3) if priority else (1 / 2,)
    picks = {ordered[round((len(ordered) - 1) * fraction)]: "interior"
             for fraction in fractions}
    picks[ordered[-1]] = "endpoint"
    return sorted(picks.items())


def _manifest_valid(path: Path, role: str, revision: str) -> bool:
    try:
        manifest = json.loads(path.read_text())
        command = manifest["command"]
        return (manifest["source_commit"] == revision
                and command[command.index("--ai-training") + 1] == role
                and command[command.index("--population") + 1] == "8"
                and all(flag in command for flag in
                        ("--cooperative-brain", "--all-scenarios", "--diverse-maps")))
    except (OSError, ValueError, KeyError, IndexError, TypeError, AttributeError):
        return False


def _active_batch(status: dict) -> tuple[str, str]:
    if status.get("state") not in ("running", "cooling", "thermal_paused"):
        return "", ""
    batch = next((part for part in Path(status.get("log", "")).parts
                  if part.startswith("batch-")), "")
    if batch.endswith(".log"):
        batch = Path(batch).stem
    return status.get("role", ""), batch


def inventory(root: Path) -> dict:
    data = root / "data"
    state = json.loads((data / "state.json").read_text())
    if state.get("protocol") != 2 or state.get("cooperative") is not True:
        raise ValueError("expected a cooperative protocol-2 deployment")
    revision = state["source_commit"]
    if not re.fullmatch(r"[0-9a-f]{40}", revision):
        raise ValueError("invalid source revision")
    hashes = _binary_hashes(root)
    status_path = data / "status.json"
    status = json.loads(status_path.read_text()) if status_path.exists() else {}
    active_role, active_batch = _active_batch(status)

    batches = []
    selections = []
    for role in ROLES:
        for batch in sorted((data / "runs" / role).glob("batch-*")):
            if not batch.is_dir() or batch.name.endswith(".tmp"):
                continue
            if not _manifest_valid(data / "manifests" / role / f"{batch.name}.json",
                                   role, revision):
                continue
            is_active = role == active_role and batch.name == active_batch
            quick_path = data / "evaluations" / role / batch.name / "quick" / "report.json"
            report = json.loads(quick_path.read_text()) if quick_path.exists() else None
            complete = (report is not None and report.get("role") == role
                        and report.get("protocol", {}).get("source_revision") == revision)
            generation_paths = sorted(batch.glob(f"{role}_generation_*.brain"))
            candidates = []
            for brain in generation_paths:
                match = re.fullmatch(rf"{role}_generation_(\d+)\.brain", brain.name)
                if not match:
                    continue
                generation = int(match[1])
                population = brain.with_suffix(".population")
                if population.is_dir():
                    candidates.append((generation, brain))
            candidates.sort()
            generations = [generation for generation, _ in candidates]
            picks = (choose_generations(generations, role in PRIORITY_ROLES)
                     if not is_active else [])
            by_generation = dict(candidates)
            valid_picks = []
            for generation, position in picks:
                brain = by_generation[generation]
                if (service.valid_checkpoint(brain, role, generation)
                        and service.population_metadata(brain.with_suffix(".population"),
                                                        role, 8)):
                    valid_picks.append((generation, position))
            batches.append({"role": role, "batch": batch.name,
                            "state": "active" if is_active else
                            "complete" if complete else "historical_partial",
                            "checkpoint_pairs": len(generations),
                            "valid_selected": len(valid_picks),
                            "first_generation": min(generations) if generations else None,
                            "last_generation": max(generations) if generations else None,
                            "quick_evaluated_generation": report.get("generation") if complete else None})
            # Active batches are changing while the service trains. A later
            # snapshot may select them after the batch is complete.
            if is_active:
                continue
            for generation, position in valid_picks:
                brain = by_generation[generation]
                evaluated = complete and report["generation"] == generation
                selections.append({"role": role, "batch": batch.name,
                                   "generation": generation,
                                   "position": position,
                                   "already_evaluated": bool(evaluated),
                                   "sha256": hashlib.sha256(brain.read_bytes()).hexdigest(),
                                   "brain": str(brain),
                                   "population": str(brain.with_suffix(".population"))})
    # The service can advance while this read-only scan runs. Exclude both
    # batches observed at the start and at the end of the scan.
    status_after = json.loads(status_path.read_text()) if status_path.exists() else {}
    later_role, later_batch = _active_batch(status_after)
    if (later_role, later_batch) != (active_role, active_batch) and later_batch:
        selections = [item for item in selections
                      if (item["role"], item["batch"]) != (later_role, later_batch)]
        for item in batches:
            if (item["role"], item["batch"]) == (later_role, later_batch):
                item["state"] = "active"
    selections.sort(key=lambda item: (item["already_evaluated"],
                                      item["role"] not in PRIORITY_ROLES,
                                      ROLES.index(item["role"]), item["generation"]))
    return {"protocol": 2, "source_revision": revision,
            "training_binary_sha256": hashes["compiled/gdp"],
            "evaluator_binary_sha256": hashes["compiled/gdp_ai_gameplay_eval"],
            "selection_rule": "interior thirds for Guard/Robot, midpoint for other roles, and batch endpoints; skip active batch",
            "batches": batches, "selections": selections}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True,
                        help="deployment root containing data/ and compiled-sha256.txt")
    args = parser.parse_args()
    print(json.dumps(inventory(args.root), indent=2))


if __name__ == "__main__":
    main()
