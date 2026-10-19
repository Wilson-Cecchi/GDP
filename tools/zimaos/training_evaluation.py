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
TEAM_PARTICIPANTS_V2 = {**TEAM_PAIRS,
                        'boss_helpers': ('robot', 'officer', 'guard')}


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


def summarize_team(directory, role, seeds, frames, source_revision, protocol_version=1):
    """Compare identical mixed-role fights and reject allied damage regressions."""
    if protocol_version not in (1, 2, 3):
        raise ValueError('unsupported team evaluation protocol')
    participants = TEAM_PAIRS if protocol_version == 1 else TEAM_PARTICIPANTS_V2
    scenarios = {name for name, pair in participants.items() if role in pair}
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
            if (len(rows) != len(participants)
                    or {row.get('scenario') for row in rows} != set(participants)):
                raise ValueError(f'incomplete team report: {path}')
            for row in rows:
                scenario = row['scenario']
                if (int(row['seed']) != seed
                        or (row['primary'], row['ally']) != participants[scenario][:2]
                        or not 1 <= int(row['elapsed_frames']) <= frames):
                    raise ValueError(f'invalid team fixture: {path}')
                if protocol_version >= 2:
                    if (row.get('protocol') != str(protocol_version)
                            or row.get('third') != (participants[scenario][2]
                                if len(participants[scenario]) == 3 else '')):
                        raise ValueError(f'invalid team protocol or third role: {path}')
                attribution = {}
                if protocol_version == 3:
                    attribution_keys = [f'{slot}_friendly_{metric}'
                                        for metric in ('hits', 'kills', 'damage')
                                        for slot in ('primary', 'ally', 'third')]
                    try:
                        attribution = {key: float(row[key]) for key in attribution_keys}
                    except (KeyError, TypeError, ValueError) as error:
                        raise ValueError(f'invalid friendly-fire attribution: {path}') from error
                    if (not all(math.isfinite(value) and value >= 0
                                for value in attribution.values())
                            or any(attribution[key] != int(attribution[key])
                                   for key in attribution_keys if key.endswith(('_hits', '_kills')))
                            or any(attribution[f'{slot}_friendly_kills']
                                   > attribution[f'{slot}_friendly_hits']
                                   for slot in ('primary', 'ally', 'third'))
                            or (len(participants[scenario]) == 2
                                and any(attribution[f'third_friendly_{metric}'] != 0
                                        for metric in ('hits', 'kills', 'damage')))):
                        raise ValueError(f'invalid friendly-fire attribution: {path}')
                if scenario not in scenarios:
                    continue
                role_slot = ('primary' if row['primary'] == role
                             else 'ally' if row['ally'] == role else 'third')
                generation_key = f'{role_slot}_generation'
                generation = int(row[generation_key])
                if policy == 'candidate':
                    generations.add(generation)
                metric_keys = ['player_damage', 'friendly_damage', 'friendly_hits',
                               'friendly_kills']
                if protocol_version >= 2:
                    metric_keys += ['enemy_shots', 'enemy_hits', 'fire_holds',
                                    'robot_player_damage', 'helpers_player_damage',
                                    'enemies_alive', 'boss_health_remaining',
                                    'first_player_hit_frame', 'boss_distance_initial',
                                    'boss_distance_minimum', 'boss_distance_final',
                                    'boss_long_range', 'boss_pressure', 'boss_pursuit']
                values = {key: float(row[key]) for key in metric_keys}
                if protocol_version == 3:
                    values.update(attribution)
                    for metric in ('hits', 'kills', 'damage'):
                        values[f'role_friendly_{metric}'] = attribution[
                            f'{role_slot}_friendly_{metric}']
                if (not all(math.isfinite(v) and v >= 0 for v in values.values())
                        or any(values[key] != int(values[key]) for key in
                               ('friendly_hits', 'friendly_kills'))
                        or (protocol_version >= 2 and
                            (abs(values['robot_player_damage']
                                 + values['helpers_player_damage']
                                 - values['player_damage']) > 0.02
                             or any(values[key] != int(values[key]) for key in
                                    ('enemy_shots', 'enemy_hits', 'fire_holds',
                                     'enemies_alive', 'first_player_hit_frame',
                                     'boss_long_range', 'boss_pressure',
                                     'boss_pursuit'))
                             or values['first_player_hit_frame']
                                > int(row['elapsed_frames'])))):
                    raise ValueError(f'invalid team metric: {path}')
                if protocol_version == 3:
                    if (any(abs(sum(attribution[f'{slot}_friendly_{metric}']
                                     for slot in ('primary', 'ally', 'third'))
                                - values[f'friendly_{metric}'])
                            > (0.02 if metric == 'damage' else 0)
                            for metric in ('hits', 'kills', 'damage'))
                            or values['friendly_kills'] > values['friendly_hits']):
                        raise ValueError(f'friendly-fire attribution total mismatch: {path}')
                samples[scenario, seed, policy] = values
    if len(generations) != 1 or next(iter(generations)) <= 0:
        raise ValueError('candidate generation changed during team evaluation')
    regressions = []
    summary = {}
    per_scenario = {}
    for policy in ('active', 'candidate'):
        rows = [samples[scenario, seed, policy]
                for scenario in sorted(scenarios) for seed in seeds]
        summary[policy] = {key: sum(row[key] for row in rows) / len(rows)
                           for key in rows[0]}
    for scenario in sorted(scenarios):
        per_scenario[scenario] = {}
        for policy in ('active', 'candidate'):
            rows = [samples[scenario, seed, policy] for seed in seeds]
            per_scenario[scenario][policy] = {
                key: sum(row[key] for row in rows) / len(rows) for key in rows[0]}
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
            if ((protocol_version < 3 and candidate['friendly_kills'] > 0)
                    or (protocol_version == 3
                        and candidate['friendly_kills'] > active['friendly_kills'])):
                regressions.append({'scenario': scenario, 'seed': seed,
                                    'metric': 'friendly_kills',
                                    'active': active['friendly_kills'],
                                    'candidate': candidate['friendly_kills']})
            if protocol_version == 3:
                if candidate['role_friendly_kills'] > 0:
                    regressions.append({'scenario': scenario, 'seed': seed,
                                        'metric': 'role_friendly_kills',
                                        'active': active['role_friendly_kills'],
                                        'candidate': candidate['role_friendly_kills']})
                if (candidate['role_friendly_damage']
                        > active['role_friendly_damage'] + 0.001):
                    regressions.append({'scenario': scenario, 'seed': seed,
                                        'metric': 'role_friendly_damage',
                                        'active': active['role_friendly_damage'],
                                        'candidate': candidate['role_friendly_damage']})
            if protocol_version >= 2 and scenario == 'boss_helpers' and role == 'robot':
                active_robot = active['robot_player_damage']
                candidate_robot = candidate['robot_player_damage']
                if candidate_robot < active_robot - max(HALF_HIT[role], active_robot * 0.10):
                    regressions.append({'scenario': scenario, 'seed': seed,
                                        'metric': 'robot_player_damage',
                                        'active': active_robot,
                                        'candidate': candidate_robot})
    score = ((summary['candidate']['player_damage'] + HALF_HIT[role])
             / (summary['active']['player_damage'] + HALF_HIT[role]))
    if (protocol_version >= 2 and role == 'robot'
            and summary['candidate']['robot_player_damage'] == 0):
        regressions.append({'scenario': 'boss_helpers', 'metric':
                            'robot_player_damage_missing', 'active':
                            summary['active']['robot_player_damage'],
                            'candidate': 0})
    return {'protocol': {'seeds': list(seeds), 'frames': frames,
                         'source_revision': source_revision,
                         'gate': f'paired-team-damage-friendly-v{protocol_version}'},
            'role': role, 'generation': generations.pop(),
            'summary': summary, 'scenarios': per_scenario,
            'regressions': regressions,
            'score': score, 'eligible': not regressions and score > 1.0001}


def combine_new_mechanics(solo, team):
    """Require complete paired solo and team evidence before archiving."""
    for key in ('role', 'generation'):
        if solo[key] != team[key]:
            raise ValueError(f'candidate {key} changed across evaluation modes')
    for key in ('seeds', 'frames', 'source_revision'):
        if solo['protocol'][key] != team['protocol'][key]:
            raise ValueError(f'evaluation {key} changed across modes')
    if team['protocol']['gate'] != 'paired-team-damage-friendly-v2':
        raise ValueError('new-mechanics evaluation needs the version 2 team fixture')
    regressions = ([{'mode': 'solo', **row} for row in solo['regressions']]
                   + [{'mode': 'team', **row} for row in team['regressions']])
    score = (solo['score'] + team['score']) / 2.0
    return {'protocol': {**{key: solo['protocol'][key]
                            for key in ('seeds', 'frames', 'source_revision')},
                         'gate': 'paired-solo-and-team-new-mechanics-v2'},
            'role': solo['role'], 'generation': solo['generation'],
            'summary': {'solo': solo['summary'], 'team': team['summary']},
            'scenarios': {'solo': solo['scenarios'], 'team': team['scenarios']},
            'regressions': regressions, 'score': score,
            'eligible': not regressions and score > 1.0001}


def summarize_investigation(directory, role, seeds, frames, source_revision):
    """Check paired lost-sight behavior and reject continuous or hidden tracking."""
    metrics = ('observer_investigate_frames', 'ally_investigate_frames',
               'observer_recover_frames', 'ally_recover_frames',
               'observer_travel', 'ally_travel', 'max_ally_knowledge_age_seconds',
               'ally_report_error', 'ally_hidden_player_error',
               'report_refreshes_without_reacquisition', 'unseen_target_updates',
               'hidden_visual_contacts')
    samples, generations, regressions = {}, set(), []
    for seed in seeds:
        for policy in ('active', 'candidate'):
            path = Path(directory) / f'{seed}-investigation-{policy}.csv'
            with path.open() as source:
                rows = [row for row in csv.DictReader(source)
                        if row.get('policy') == 'neural']
            if (len(rows) != 1 or rows[0].get('scenario') != 'lost_sight'
                    or rows[0].get('archetype') != role
                    or int(rows[0]['seed']) != seed):
                raise ValueError(f'invalid investigation fixture: {path}')
            row = rows[0]
            generation = int(row['brain_generation'])
            if policy == 'candidate':
                generations.add(generation)
            values = {key: float(row[key]) for key in metrics}
            if not all(math.isfinite(value) and value >= 0 for value in values.values()):
                raise ValueError(f'invalid investigation metric: {path}')
            elapsed = int(row['elapsed_frames'])
            if (not 31 <= elapsed <= frames
                    or any(values[key] != int(values[key]) for key in metrics[:4]
                           + metrics[-3:])):
                raise ValueError(f'incomplete investigation: {path}')
            contract = {
                'first_observer_sight_frame': int(row['first_observer_sight_frame']),
                'first_ally_report_frame': int(row['first_ally_report_frame']),
                'observer_sight_frames': int(row['observer_sight_frames']),
                'ally_sight_frames': int(row['ally_sight_frames']),
                'ally_source_at_report': row['ally_source_at_report'],
                'ally_source_final': row['ally_source_final'],
                'max_ally_knowledge_age_seconds': values['max_ally_knowledge_age_seconds'],
                'ally_hidden_player_error': values['ally_hidden_player_error'],
            }
            violations = {}
            for key, valid in (
                    ('first_observer_sight_frame', contract['first_observer_sight_frame'] >= 1),
                    ('first_ally_report_frame', contract['first_ally_report_frame'] >= 1),
                    ('observer_sight_frames', contract['observer_sight_frames'] >= 30),
                    ('ally_sight_frames', contract['ally_sight_frames'] == 0),
                    ('ally_source_at_report', contract['ally_source_at_report'] == 'ally_report'),
                    ('ally_source_final', contract['ally_source_final'] == 'none'),
                    ('max_ally_knowledge_age_seconds', contract['max_ally_knowledge_age_seconds'] >= 1.0),
                    ('ally_hidden_player_error', contract['ally_hidden_player_error'] >= 1.0)):
                if not valid:
                    violations[key] = contract[key]
            if violations:
                if policy == 'active':
                    raise ValueError(f'incomplete investigation: {path}')
                regressions.append({'seed': seed, 'metric': 'investigation_contract',
                                    'values': violations})
            if any(values[key] != 0 for key in metrics[-3:]):
                regressions.append({'seed': seed, 'policy': policy,
                                    'metric': 'information_leak',
                                    'values': {key: values[key] for key in metrics[-3:]}})
            samples[seed, policy] = values
    if len(generations) != 1 or next(iter(generations)) <= 0:
        raise ValueError('candidate generation changed during investigation')
    for seed in seeds:
        active, candidate = samples[seed, 'active'], samples[seed, 'candidate']
        for key in ('observer_investigate_frames', 'ally_investigate_frames'):
            if active[key] < 10:
                raise ValueError(f'active investigation fixture did not exercise {key}')
            if candidate[key] < max(10.0, active[key] * 0.5):
                regressions.append({'seed': seed, 'metric': key,
                                    'active': active[key], 'candidate': candidate[key]})
        for key in ('observer_travel', 'ally_travel'):
            if candidate[key] < max(0.5, active[key] * 0.5):
                regressions.append({'seed': seed, 'metric': key,
                                    'active': active[key], 'candidate': candidate[key]})
    summary = {policy: {key: sum(samples[seed, policy][key] for seed in seeds) / len(seeds)
                        for key in metrics}
               for policy in ('active', 'candidate')}
    score = ((summary['candidate']['observer_travel'] + summary['candidate']['ally_travel'])
             / max(0.001, summary['active']['observer_travel']
                   + summary['active']['ally_travel']))
    return {'protocol': {'seeds': list(seeds), 'frames': frames,
                         'source_revision': source_revision,
                         'gate': 'paired-lost-sight-information-v3'},
            'role': role, 'generation': generations.pop(), 'summary': summary,
            'scenarios': {'lost_sight': {str(seed):
                {policy: samples[seed, policy] for policy in ('active', 'candidate')}
                for seed in seeds}}, 'regressions': regressions,
            'score': score, 'eligible': not regressions}


def combine_perception(solo, team, investigation, *, team_protocol=2):
    """Keep protocol 3 evaluation distinct from old candidate archives."""
    if team_protocol not in (2, 3):
        raise ValueError('unsupported combined team protocol')
    for report in (team, investigation):
        for key in ('role', 'generation'):
            if solo[key] != report[key]:
                raise ValueError(f'candidate {key} changed across evaluation modes')
        for key in ('seeds', 'frames', 'source_revision'):
            if solo['protocol'][key] != report['protocol'][key]:
                raise ValueError(f'evaluation {key} changed across modes')
    if (team['protocol']['gate'] != f'paired-team-damage-friendly-v{team_protocol}'
            or investigation['protocol']['gate'] != 'paired-lost-sight-information-v3'):
        raise ValueError('perception evaluation requires matching team and investigation gates')
    regressions = ([{'mode': 'solo', **row} for row in solo['regressions']]
                   + [{'mode': 'team', **row} for row in team['regressions']]
                   + [{'mode': 'investigation', **row}
                      for row in investigation['regressions']])
    score = (solo['score'] + team['score']) / 2.0
    return {'protocol': {**{key: solo['protocol'][key]
                            for key in ('seeds', 'frames', 'source_revision')},
                         'gate': ('paired-solo-team-perception-v3' if team_protocol == 2
                                  else 'paired-solo-team-perception-attributed-v4')},
            'role': solo['role'], 'generation': solo['generation'],
            'summary': {'solo': solo['summary'], 'team': team['summary'],
                        'investigation': investigation['summary']},
            'scenarios': {'solo': solo['scenarios'], 'team': team['scenarios'],
                          'investigation': investigation['scenarios']},
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
