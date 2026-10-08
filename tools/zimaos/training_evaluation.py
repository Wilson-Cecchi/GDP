"""Paired gameplay reports and an archive of candidates; never promotes assets."""
import csv
import hashlib
import json
import math
from pathlib import Path
import shutil

HALF_HIT = {'worker': 5.0, 'guard': 4.0, 'officer': 3.0, 'engineer': 2.5, 'robot': 3.0}


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(value, indent=2) + '\n')
    temporary.replace(path)


def summarize(directory, role, seeds, frames, source_revision):
    """Require every scenario/seed pair; reject truncated, duplicate or NaN rows."""
    scenario_sets = {'maps': {'open', 'center_cover', 'elevated', 'group'},
                     'duel': {'ground_strafe', 'bhop'}}
    if role == 'robot':
        scenario_sets['maps'].add('boss_cover')
    pairs = {}
    generations = set()
    for mode, scenarios in scenario_sets.items():
        for policy in ('active', 'candidate'):
            samples = {}
            for seed in seeds:
                path = Path(directory) / f'{seed}-{mode}-{policy}.csv'
                with path.open() as source:
                    rows = [r for r in csv.DictReader(source)
                            if r.get('policy') == 'neural' and r.get('archetype') == role]
                if len(rows) != len(scenarios) or {r.get('scenario') for r in rows} != scenarios:
                    raise ValueError(f'incomplete gameplay report: {path}')
                for row in rows:
                    if int(row['seed']) != seed:
                        raise ValueError(f'seed mismatch: {path}')
                    generation = int(row['brain_generation'])
                    if policy == 'candidate':
                        generations.add(generation)
                    key = (row['scenario'], seed)
                    damage_key = 'player_damage' if mode == 'maps' else 'damage_to_player'
                    damage, travel = float(row[damage_key]), float(row['enemy_travel'])
                    if not all(math.isfinite(v) and v >= 0 for v in (damage, travel)):
                        raise ValueError(f'invalid metric: {path}')
                    if not 1 <= int(row['elapsed_frames']) <= frames:
                        raise ValueError(f'invalid duration: {path}')
                    samples[key] = {'damage': damage, 'travel': travel,
                                    'outcome': row.get('outcome')}
            pairs[mode, policy] = samples
    if len(generations) != 1:
        raise ValueError('candidate generation changed during evaluation')
    regressions, summary, per_scenario = [], {}, {}
    for mode, scenarios in scenario_sets.items():
        for policy in ('active', 'candidate'):
            values = list(pairs[mode, policy].values())
            summary[f'{mode}_{policy}'] = {key: sum(r[key] for r in values) / len(values)
                                          for key in ('damage', 'travel')}
        for scenario in sorted(scenarios):
            values = {}
            for policy in ('active', 'candidate'):
                values[policy] = {key: sum(pairs[mode, policy][scenario, seed][key]
                    for seed in seeds) / len(seeds) for key in ('damage', 'travel')}
            per_scenario[f'{mode}:{scenario}'] = values
            # Check each paired seed as well as the mean; averages cannot hide a bad fixture.
            for seed in seeds:
                active = pairs[mode, 'active'][scenario, seed]['damage']
                candidate = pairs[mode, 'candidate'][scenario, seed]['damage']
                tolerance = max(HALF_HIT[role], active * 0.10)
                if candidate < active - tolerance:
                    regressions.append({'mode': mode, 'scenario': scenario, 'seed': seed,
                                        'active': active, 'candidate': candidate})
    ratios = []
    for mode in scenario_sets:
        active = summary[f'{mode}_active']['damage']
        candidate = summary[f'{mode}_candidate']['damage']
        ratios.append((candidate + HALF_HIT[role]) / (active + HALF_HIT[role]))
    score = 0.6 * ratios[0] + 0.4 * ratios[1]
    return {'protocol': {'seeds': list(seeds), 'frames': frames, 'source_revision': source_revision,
                         'gate': 'per-seed-damage-v1'}, 'role': role,
            'generation': generations.pop(), 'summary': summary, 'scenarios': per_scenario,
            'regressions': regressions, 'score': score,
            'eligible': not regressions and score > 1.0001}


def archive_candidate(report, champion, data, label):
    """Preserve every accepted contender; maintain a best index per fixed protocol."""
    protocol_hash = hashlib.sha256(json.dumps(report['protocol'], sort_keys=True).encode()).hexdigest()[:12]
    root = Path(data) / 'candidates' / f'{label}-{protocol_hash}' / report['role']
    root.mkdir(parents=True, exist_ok=True)
    atomic_json(root / 'latest.json', report)
    if not report['eligible']:
        return False
    champion = Path(champion)
    original_digest = hashlib.sha256(champion.read_bytes()).hexdigest()
    selected = root / f'{champion.parent.name}-{original_digest[:12]}' / champion.name
    selected.parent.mkdir(parents=True, exist_ok=True)
    if not selected.exists():
        temporary = selected.with_suffix('.tmp')
        shutil.copyfile(champion, temporary)
        temporary.replace(selected)
    digest = hashlib.sha256(selected.read_bytes()).hexdigest()
    if digest != original_digest:
        raise ValueError('candidate archive content does not match the evaluated champion')
    report = {**report, 'brain': str(selected), 'brain_sha256': digest,
              'source_brain': str(champion)}
    atomic_json(selected.with_suffix('.json'), report)
    best_file = root / 'best.json'
    best = json.loads(best_file.read_text()) if best_file.exists() else None
    if best:
        best_brain = Path(best['brain'])
        if not best_brain.exists() or hashlib.sha256(best_brain.read_bytes()).hexdigest() != best['brain_sha256']:
            raise ValueError('archived best candidate is missing or changed')
    if not best or report['score'] > best['score'] + 0.0001:
        atomic_json(best_file, report)
        return True
    return False
