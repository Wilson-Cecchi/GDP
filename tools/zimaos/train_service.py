#!/usr/bin/env python3
"""Run sequential, checkpointed GDP training batches with a thermal guard."""

from __future__ import annotations

import fcntl
import hashlib
import json
import math
import os
from pathlib import Path
import re
import shutil
import signal
import struct
import subprocess
import tempfile
import time

from training_evaluation import summarize, summarize_team, archive_candidate

ROLES = ("worker", "guard", "officer", "engineer", "robot")
COOPERATIVE = os.environ.get("GDP_COOPERATIVE", "0") == "1"
DATA = Path(os.environ.get("GDP_DATA_DIR", "/data"))
APP = Path(os.environ.get("GDP_APP_DIR", "/opt/gdp"))
HWMON = Path(os.environ.get("GDP_HWMON_DIR", "/host-sys/class/hwmon"))
STOP = False


def event(name: str, **fields: object) -> None:
    print(json.dumps({"event": name, "time_utc": time.strftime(
        "%Y-%m-%dT%H:%M:%SZ", time.gmtime()), **fields}), flush=True)


def atomic_json(path: Path, value: object) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n")
    temporary.replace(path)


def temperature() -> float:
    readings = []
    for directory in HWMON.glob("hwmon*"):
        try:
            if (directory / "name").read_text().strip() != "coretemp":
                continue
            for sensor in directory.glob("temp*_input"):
                value = int(sensor.read_text().strip()) / 1000.0
                if 0.0 < value < 130.0:
                    readings.append(value)
        except (OSError, ValueError):
            continue
    if not readings:
        raise RuntimeError("CPU temperature sensor unavailable; training stopped")
    return max(readings)


def valid_checkpoint(path: Path, role: str, expected_generation: int | None = None) -> bool:
    """Check the selected brain contract before unattended resume."""
    try:
        raw = path.read_bytes()
        if len(raw) > 16384:
            return False
        canonical = raw.replace(b"\r\n", b"\n")
        if b"\r" in canonical:
            return False
        body, checksum = canonical.rsplit(b"checksum ", 1)
        if not body.endswith(b"\n"):
            return False
        digest = 14695981039346656037
        for byte in body:
            digest = ((digest ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        if int(checksum.strip()) != digest:
            return False
        lines = body.decode("ascii").splitlines()
        if lines[0] != ("GDP_ENEMY_BRAIN 4" if COOPERATIVE else "GDP_ENEMY_BRAIN 3"):
            return False
        keys = ("archetype", "contract", "seed", "generation", "id",
                "fitness_bits", "weights")
        values = {}
        for line, key in zip(lines[1:8], keys):
            label, value = line.split()
            if label != key:
                return False
            values[key] = int(value)
        expected_weights = 305 if COOPERATIVE else 221
        if (values["archetype"] != ROLES.index(role)
                or values["contract"] != (3 if COOPERATIVE else 2)
                or values["weights"] != expected_weights
                or len(lines) != expected_weights + 8):
            return False
        if not (0 <= values["seed"] < 2**64
                and 0 <= values["generation"] < 2**32
                and 0 <= values["id"] < 2**32):
            return False
        fitness = struct.unpack("!f", struct.pack("!I", values["fitness_bits"]))[0]
        if not math.isfinite(fitness):
            return False
        for line in lines[8:]:
            weight = struct.unpack("!f", struct.pack("!I", int(line)))[0]
            if not math.isfinite(weight) or not -1.0 <= weight <= 1.0:
                return False
        match = re.fullmatch(rf"{role}_generation_(\d+)\.brain", path.name)
        return (values["generation"] == expected_generation if expected_generation is not None
                else match is not None and int(match[1]) == values["generation"])
    except (OSError, ValueError, UnicodeError, IndexError, KeyError, struct.error):
        return False


def latest_checkpoint(directory: Path, role: str) -> Path | None:
    paths = sorted(directory.rglob(f"{role}_generation_*.brain"),
                   key=lambda p: (int(p.stem.rsplit("_", 1)[1]), str(p)), reverse=True)
    for path in paths:
        if valid_checkpoint(path, role):
            return path
        event("invalid_checkpoint", path=str(path))
    if paths:
        raise RuntimeError(f"No valid resume checkpoint remains for {role}")
    return None


def population_metadata(path: Path, role: str, population: int) -> dict | None:
    try:
        raw = (path / "state.txt").read_bytes()
        if len(raw) > 4096:
            return None
        body, checksum = raw.rsplit(b"checksum ", 1)
        digest = 14695981039346656037
        for byte in body:
            digest = ((digest ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        if int(checksum.strip()) != digest:
            return None
        lines = body.decode("ascii").splitlines()
        keys = ("archetype", "seed", "generation", "population", "reward_rules")
        if COOPERATIVE:
            keys += ("contract",)
        keys += ("curriculum", "warmup_start", "warmup_generations")
        if (len(lines) != len(keys) + 1
                or lines[0] != f"GDP_TRAINING_POPULATION {2 if COOPERATIVE else 1}"):
            return None
        fields = {}
        for line, expected in zip(lines[1:], keys):
            key, value = line.split()
            if key != expected:
                return None
            fields[key] = int(value)
        if (fields["archetype"] != ROLES.index(role) or fields["population"] != population
                or fields["curriculum"] != 15
                or fields["reward_rules"] != (6 if COOPERATIVE else 5)
                or (COOPERATIVE and fields["contract"] != 3)
                or not 0 < fields["generation"] < 2**32
                or not 0 <= fields["seed"] < 2**64
                or not 0 < fields["warmup_start"] <= fields["generation"]
                or not 0 <= fields["warmup_generations"] <= 10000
                or path.name != f'{role}_generation_{fields["generation"]:06d}.population'):
            return None
        ids, ordering = set(), []
        for index in range(population):
            member = path / f"member-{index:03d}.brain"
            if not valid_checkpoint(member, role, fields["generation"]):
                return None
            metadata = dict(line.split() for line in member.read_text().splitlines()[1:7])
            identity = int(metadata["id"])
            if int(metadata["seed"]) != fields["seed"] or identity in ids or not 0 <= identity < population:
                return None
            ids.add(identity)
            fitness = struct.unpack("!f", struct.pack("!I", int(metadata["fitness_bits"])))[0]
            ordering.append((-fitness, identity))
        return fields if ordering == sorted(ordering) else None
    except (OSError, ValueError, UnicodeError, KeyError, struct.error):
        return None


def latest_population(directory: Path, role: str, population: int) -> Path | None:
    paths = sorted(directory.rglob(f"{role}_generation_*.population"),
                   key=lambda p: (int(p.stem.rsplit("_", 1)[1]), str(p)), reverse=True)
    for path in paths:
        if population_metadata(path, role, population):
            return path
        event("invalid_population", path=str(path))
    if paths:
        raise RuntimeError(f"No compatible complete population remains for {role}")
    return None


def evaluate_candidate(role: str, champion: Path, directory: Path, seeds: tuple[int, ...],
                       frames: int, source_commit: str, pause_c: float, resume_c: float,
                       label: str) -> dict | None:
    if COOPERATIVE:
        for eval_seed in seeds:
            for candidate in (False, True):
                if STOP:
                    return None
                policy = "candidate" if candidate else "active"
                log = directory / f"{eval_seed}-team-{policy}.csv"
                command = [str(APP / "gdp_ai_gameplay_eval"), "--brain-dir",
                           str(APP / "assets/ai"), "--seed", str(eval_seed),
                           "--frames", str(frames), "--team"]
                if candidate:
                    command += ["--brain", f"{role}={champion}"]
                if run(command, log, role, "evaluation", pause_c, resume_c) != 0:
                    if STOP:
                        return None
                    raise RuntimeError(f"Team evaluation failed for {role}")
        if STOP:
            return None
        report = summarize_team(directory, role, seeds, frames, source_commit)
        atomic_json(directory / "report.json", report)
        improved = archive_candidate(report, champion, DATA, label)
        event("candidate_evaluated", role=role, label=label,
              generation=report["generation"], eligible=report["eligible"],
              new_archived_best=improved, regressions=len(report["regressions"]),
              score=report["score"])
        return report
    for eval_seed in seeds:
        for duel in (False, True):
            for candidate in (False, True):
                if STOP:
                    return None
                mode = "duel" if duel else "maps"
                policy = "candidate" if candidate else "active"
                log = directory / f"{eval_seed}-{mode}-{policy}.csv"
                command = [str(APP / "gdp_ai_gameplay_eval"), "--brain-dir",
                           str(APP / "assets/ai"), "--seed", str(eval_seed),
                           "--frames", str(frames)]
                if duel:
                    command.append("--duel")
                if candidate:
                    command += ["--brain", f"{role}={champion}"]
                if run(command, log, role, "evaluation", pause_c, resume_c) != 0:
                    if STOP:
                        return None
                    raise RuntimeError(f"Evaluation failed for {role}")
    if STOP:
        return None
    report = summarize(directory, role, seeds, frames, source_commit)
    atomic_json(directory / "report.json", report)
    improved = archive_candidate(report, champion, DATA, label)
    event("candidate_evaluated", role=role, label=label, generation=report["generation"],
          eligible=report["eligible"], new_archived_best=improved,
          regressions=len(report["regressions"]), score=report["score"])
    return report


def request_stop(_signum: int, _frame: object) -> None:
    global STOP
    STOP = True


def run(command: list[str], log: Path, role: str, phase: str,
        pause_c: float, resume_c: float) -> int:
    cooling = temperature() >= pause_c
    while cooling and not STOP:
        current = temperature()
        atomic_json(DATA / "status.json", {
            "state": "cooling", "role": role, "phase": phase,
            "temperature_c": current, "log": str(log)})
        cooling = current > resume_c
        if cooling:
            time.sleep(5)
    if STOP:
        return 143
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w") as output:
        child = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT)
        paused = False
        event("process_start", role=role, phase=phase, pid=child.pid, log=str(log))
        try:
            while child.poll() is None:
                current = temperature()
                if current >= pause_c and not paused:
                    child.send_signal(signal.SIGSTOP)
                    paused = True
                    event("thermal_pause", temperature_c=current, role=role)
                elif paused and current <= resume_c:
                    child.send_signal(signal.SIGCONT)
                    paused = False
                    event("thermal_resume", temperature_c=current, role=role)
                atomic_json(DATA / "status.json", {
                    "state": "thermal_paused" if paused else "running",
                    "role": role, "phase": phase, "pid": child.pid,
                    "temperature_c": current, "log": str(log)})
                if STOP:
                    break
                time.sleep(2)
        finally:
            if child.poll() is None:
                if paused:
                    child.send_signal(signal.SIGCONT)
                child.terminate()
                try:
                    child.wait(timeout=15)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()
        return child.returncode


def bounded_integer(name: str, default: int, low: int, high: int) -> int:
    value = int(os.environ.get(name, str(default)))
    if not low <= value <= high:
        raise ValueError(f"{name} must be {low}..{high}")
    return value


def main() -> None:
    DATA.mkdir(parents=True, exist_ok=True)
    with (DATA / "service.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        population = bounded_integer("GDP_POPULATION", 8, 2, 128)
        generations = bounded_integer("GDP_BATCH_GENERATIONS", 70, 7, 10000)
        seed = bounded_integer("GDP_SEED", 20261001, 0, 2**64 - 1)
        cooldown = bounded_integer("GDP_COOLDOWN_MS", 1000, 0, 10000)
        pause_c = float(os.environ.get("GDP_PAUSE_C", "80"))
        resume_c = float(os.environ.get("GDP_RESUME_C", "70"))
        if not (0 < resume_c < pause_c < 100):
            raise ValueError("thermal thresholds must satisfy 0 < resume < pause < 100")
        source_commit = (APP / "SOURCE_COMMIT").read_text().strip()
        train_roles = tuple(role.strip() for role in os.environ.get(
            "GDP_TRAIN_ROLES", "worker,guard,officer,engineer" if COOPERATIVE
            else ",".join(ROLES)).split(","))
        if (not train_roles or len(set(train_roles)) != len(train_roles)
                or any(role not in ROLES for role in train_roles)
                or (COOPERATIVE and "robot" in train_roles)):
            raise ValueError("Invalid GDP_TRAIN_ROLES; cooperative team evaluation covers worker, guard, officer, engineer")
        wide_every = bounded_integer("GDP_WIDE_EVERY", 5, 1, 100)
        state_file = DATA / "state.json"
        state = json.loads(state_file.read_text()) if state_file.exists() else {}
        if state.get("train_roles") and state["train_roles"] != list(train_roles):
            raise ValueError("Training role rotation changed; use a separate data directory")
        if state.get("cooperative") is not None and state["cooperative"] != COOPERATIVE:
            raise ValueError("Brain contract changed; use a separate data directory")
        next_role = int(state.get("next_role", 0)) % len(train_roles)
        completed = state.get("completed", {})
        event("service_start", source_commit=source_commit, population=population,
              generations=generations, temperature_c=temperature())
        while not STOP:
            if shutil.disk_usage(DATA).free < 1024**3:
                raise RuntimeError("Less than 1 GiB free on training data storage")
            role = train_roles[next_role]
            role_runs = DATA / "runs" / role
            role_runs.mkdir(parents=True, exist_ok=True)
            full_resume = latest_population(role_runs, role, population)
            resume = None if full_resume else latest_checkpoint(role_runs, role)
            batch = Path(tempfile.mkdtemp(prefix="batch-", dir=role_runs))
            command = [str(APP / "gdp"), "--ai-training", role,
                       "--all-scenarios", "--diverse-maps", "--population", str(population),
                       "--generations", str(generations), "--cooldown-ms",
                       str(cooldown), "--save-dir", str(batch)]
            if COOPERATIVE:
                command.append("--cooperative-brain")
            if full_resume:
                command += ["--resume-population", str(full_resume)]
            elif resume:
                command += ["--resume", str(resume)]
                event("champion_import", role=role, path=str(resume))
            else:
                command += ["--seed", str(seed)]
            manifest = DATA / "manifests" / role / f"{batch.name}.json"
            manifest.parent.mkdir(parents=True, exist_ok=True)
            atomic_json(manifest, {"source_commit": source_commit, "command": command,
                "resume_sha256": hashlib.sha256(resume.read_bytes()).hexdigest()
                if resume else None, "resume_population": str(full_resume) if full_resume else None})
            code = run(command, DATA / "logs" / role / f"{batch.name}.log",
                       role, "training", pause_c, resume_c)
            if STOP:
                break
            if code != 0:
                raise RuntimeError(f"Training failed for {role} with exit {code}")
            champion = latest_checkpoint(batch, role)
            if champion is None:
                raise RuntimeError(f"Training produced no valid checkpoint for {role}")
            evaluation_root = DATA / "evaluations" / role / batch.name
            evaluate_candidate(role, champion, evaluation_root / "quick",
                               (9001, 9002) if COOPERATIVE else (7, 1001),
                               480 if COOPERATIVE else 360, source_commit,
                               pause_c, resume_c,
                               "quick-coop-v1" if COOPERATIVE else "quick-v2")
            role_completed = int(completed.get(role, 0)) + 1
            if not STOP and (role_completed == 1 or role_completed % wide_every == 0):
                # Development validation only. Final seeds are reserved for a separate run.
                evaluate_candidate(role, champion, evaluation_root / "wide",
                                   (9001, 9002, 12011, 14009) if COOPERATIVE
                                   else (7, 1001, 2027, 4099),
                                   1200, source_commit, pause_c, resume_c,
                                   "wide-coop-v1" if COOPERATIVE else "wide-v2")
            if STOP:
                break
            event("batch_complete", role=role, champion=str(champion))
            completed[role] = role_completed
            next_role = (next_role + 1) % len(train_roles)
            atomic_json(state_file, {"next_role": next_role,
                                    "source_commit": source_commit, "completed": completed,
                                    "cooperative": COOPERATIVE,
                                    "train_roles": list(train_roles)})
        atomic_json(DATA / "status.json", {"state": "stopped"})
        event("service_stop")


if __name__ == "__main__":
    signal.signal(signal.SIGTERM, request_stop)
    signal.signal(signal.SIGINT, request_stop)
    try:
        main()
    except Exception as error:
        event("service_error", error=str(error))
        if DATA.exists():
            atomic_json(DATA / "status.json", {"state": "error", "error": str(error)})
        time.sleep(30)
        raise SystemExit(1)
