"""Paired gameplay reports and an archive of candidates; never promotes assets."""
import csv
import hashlib
import json
import math
from pathlib import Path
import shutil

HALF_HIT = {'worker': 5.0, 'guard': 4.0, 'officer': 3.0, 'engineer': 2.5, 'robot': 3.0}
TEAM_PAIRS = {'firing_lane': ('guard', 'worker'),
              'officer_guard': ('officer', 'guard'),
              'engineer_guard': ('engineer', 'guard')}


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


def summarize_team(directory, role, seeds, frames, source_revision):
    """Compare identical mixed-role fights and reject allied damage regressions."""
    scenarios = {name for name, pair in TEAM_PAIRS.items() if role in pair}
    if not scenarios:
        raise ValueError(f'No team evaluation fixture covers {role}')
    samples = {}
    generations = set()
    for seed in seeds:
        for policy in ('active', 'candidate'):
            path = Path(directory) / f'{seed}-team-{policy}.csv'
            with path.open() as source:
                rows = [row for row in csv.DictReader(source)
                        if row.get('policy') == 'neural']
            if (len(rows) != len(TEAM_PAIRS)
                    or {row.get('scenario') for row in rows} != set(TEAM_PAIRS)):
                raise ValueError(f'incomplete team report: {path}')
            for row in rows:
                scenario = row['scenario']
                if (int(row['seed']) != seed
                        or (row['primary'], row['ally']) != TEAM_PAIRS[scenario]
                        or not 1 <= int(row['elapsed_frames']) <= frames):
                    raise ValueError(f'invalid team fixture: {path}')
                if scenario not in scenarios:
                    continue
                generation = int(row['primary_generation'] if row['primary'] == role
                                 else row['ally_generation'])
                if policy == 'candidate':
                    generations.add(generation)
                values = {key: float(row[key]) for key in
                          ('player_damage', 'friendly_damage', 'friendly_hits',
                           'friendly_kills')}
                if (not all(math.isfinite(v) and v >= 0 for v in values.values())
                        or any(values[key] != int(values[key]) for key in
                               ('friendly_hits', 'friendly_kills'))):
                    raise ValueError(f'invalid team metric: {path}')
                samples[scenario, seed, policy] = values
    if len(generations) != 1 or next(iter(generations)) <= 0:
        raise ValueError('candidate generation changed during team evaluation')
    regressions = []
    summary = {}
    for policy in ('active', 'candidate'):
        rows = [samples[scenario, seed, policy]
                for scenario in sorted(scenarios) for seed in seeds]
        summary[policy] = {key: sum(row[key] for row in rows) / len(rows)
                           for key in rows[0]}
    for scenario in sorted(scenarios):
        for seed in seeds:
            active = samples[scenario, seed, 'active']
            candidate = samples[scenario, seed, 'candidate']
            tolerance = max(HALF_HIT[role], active['player_damage'] * 0.10)
            if candidate['player_damage'] < active['player_damage'] - tolerance:
                regressions.append({'scenario': scenario, 'seed': seed,
                                    'metric': 'player_damage',
                                    'active': active['player_damage'],
                                    'candidate': candidate['player_damage']})
            if candidate['friendly_damage'] > active['friendly_damage'] + 0.001:
                regressions.append({'scenario': scenario, 'seed': seed,
                                    'metric': 'friendly_damage',
                                    'active': active['friendly_damage'],
                                    'candidate': candidate['friendly_damage']})
            if candidate['friendly_kills'] > 0:
                regressions.append({'scenario': scenario, 'seed': seed,
                                    'metric': 'friendly_kills',
                                    'active': active['friendly_kills'],
                                    'candidate': candidate['friendly_kills']})
    score = ((summary['candidate']['player_damage'] + HALF_HIT[role])
             / (summary['active']['player_damage'] + HALF_HIT[role]))
    return {'protocol': {'seeds': list(seeds), 'frames': frames,
                         'source_revision': source_revision,
                         'gate': 'paired-team-damage-friendly-v1'},
            'role': role, 'generation': generations.pop(),
            'summary': summary, 'regressions': regressions,
            'score': score, 'eligible': not regressions and score > 1.0001}


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
