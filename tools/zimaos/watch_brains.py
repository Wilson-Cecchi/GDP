#!/usr/bin/env python3
"""Compact live view of the isolated cooperative training service."""

import json
from pathlib import Path
import time

ROOT = Path("/DATA/AppData/gdp-coop-training")
DATA = ROOT / "data"


def tail(path: Path) -> str:
    with path.open("rb") as source:
        source.seek(max(0, path.stat().st_size - 8192))
        lines = source.read().decode("utf-8", "replace").splitlines()
    return lines[-1] if lines else ""


def main() -> None:
    status_file = DATA / "status.json"
    status = json.loads(status_file.read_text())
    age = max(0, int(time.time() - status_file.stat().st_mtime))
    state_file = DATA / "state.json"
    state = json.loads(state_file.read_text()) if state_file.exists() else {}
    print("GDP: treino cooperativo no ZimaOS")
    print(f"Estado: {status.get('state')} | Tipo: {status.get('role')} | "
          f"Fase: {status.get('phase')} | CPU: {status.get('temperature_c')} °C")
    print(f"Status atualizado há {age} s | Lotes completos: "
          + ", ".join(f"{role}={count}" for role, count in
                      state.get("completed", {}).items()))
    if age > 300:
        print("ATENÇÃO: status sem atualização há mais de cinco minutos")
    log = status.get("log", "")
    if not log.startswith("/data/"):
        return
    path = DATA / log.removeprefix("/data/")
    print(f"Arquivo: {path}")
    if not path.is_file():
        return
    last = tail(path)
    if last.startswith(("AI_GENERATION ", "AI_RESULT ")):
        fields = dict(part.split("=", 1) for part in last.split()[1:]
                      if "=" in part)
        keys = ("number", "generation", "stage", "best", "holdout",
                "damage_dealt", "friendly_hits", "friendly_damage",
                "friendly_fire_holds")
        print(" | ".join(f"{key}={fields[key]}" for key in keys if key in fields))
    else:
        print(last[:220])
    print("Ctrl+C para sair")


if __name__ == "__main__":
    main()
