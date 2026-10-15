#!/usr/bin/env python3
"""Read-only, provisional assessment of a paired five-brain evaluation.

This tool reports evidence for review. It never selects or promotes a brain.
"""

import argparse
import csv
import json
import math
from pathlib import Path
import re


ROLES = ('worker', 'guard', 'officer', 'engineer', 'robot')
MAP_SCENARIOS = ('open', 'center_cover', 'elevated', 'group')
DUEL_SCENARIOS = ('ground_strafe', 'bhop')
TEAM_SCENARIOS = {
    'firing_lane': ('guard', 'worker'),
    'officer_guard': ('officer', 'guard'),
    'engineer_guard': ('engineer', 'guard'),
    'boss_helpers': ('robot', 'officer', 'guard'),
}
HALF_HIT = {'worker': 5.0, 'guard': 4.0, 'officer': 3.0,
            'engineer': 2.5, 'robot': 3.0}
FINAL_SEEDS = {151001, 151013, 151027, 151039,
               151051, 151061, 151073, 151099}
DEV_SEEDS = {'quick': [150001, 150011],
             'wide': [150001, 150011, 150031, 150053]}
STAGE_FRAMES = {'quick': 480, 'wide': 1200}
ROLE_TELEMETRY = tuple(f'{position}_{metric}'
    for metric in ('shots', 'player_hits', 'fire_holds')
    for position in ('primary', 'ally', 'third'))
TEXT_COLUMNS = {'policy', 'scenario', 'archetype', 'outcome',
                'primary', 'ally', 'third'}
HASH = re.compile(r'[0-9a-f]{64}\Z')


def _integer(value, context, minimum=0):
    try:
        number = int(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f'invalid integer {context}: {value}') from error
    if number < minimum:
        raise ValueError(f'invalid integer {context}: {value}')
    return number


def _number(value, context):
    try:
        number = float(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f'invalid number {context}: {value}') from error
    if not math.isfinite(number) or number < 0:
        raise ValueError(f'invalid number {context}: {value}')
    return number


def _manifest(directory):
    manifest = json.loads((directory / 'manifest.json').read_text())
    if not isinstance(manifest, dict) or manifest.get('complete') is not True:
        raise ValueError('bundle manifest is incomplete')
    seeds = manifest.get('seeds')
    if (not isinstance(seeds, list) or len(seeds) not in (2, 4)
            or any(type(seed) is not int or seed <= 0 for seed in seeds)
            or len(set(seeds)) != len(seeds)
            or FINAL_SEEDS.intersection(seeds)):
        raise ValueError('invalid or reserved evaluation seeds')
    frames = _integer(manifest.get('frames'), 'manifest frames', 1)
    if manifest.get('stage') not in STAGE_FRAMES \
            or frames != STAGE_FRAMES[manifest['stage']]:
        raise ValueError('invalid evaluation stage or frames')
    if seeds != DEV_SEEDS[manifest['stage']] \
            or manifest.get('modes') != ['maps', 'duel', 'team']:
        raise ValueError('stage, seeds and modes disagree')
    revision = manifest.get('evaluator_source_revision')
    if not isinstance(revision, str) or not revision.strip():
        raise ValueError('missing evaluator source revision')
    if not isinstance(manifest.get('evaluator_sha256'), str) or not HASH.fullmatch(
            manifest['evaluator_sha256']):
        raise ValueError('invalid evaluator hash')
    active = manifest.get('active_brain_sha256')
    bundle = manifest.get('candidate_brains')
    if not isinstance(active, dict) or not isinstance(bundle, dict) \
            or set(active) != set(ROLES) or set(bundle) != set(ROLES):
        raise ValueError('manifest requires five active and five bundle brains')
    for role in ROLES:
        if not isinstance(active[role], str) or not HASH.fullmatch(active[role]):
            raise ValueError(f'invalid active brain hash: {role}')
        record = bundle[role]
        if (not isinstance(record, dict) or not isinstance(record.get('path'), str)
                or not record['path'] or not isinstance(record.get('sha256'), str)
                or not HASH.fullmatch(record['sha256'])):
            raise ValueError(f'invalid bundle brain record: {role}')
    expected = {f'{seed}-{mode}-{arm}.csv' for seed in seeds
                for mode in ('maps', 'duel', 'team')
                for arm in ('active', 'bundle')}
    completed = manifest.get('completed')
    if not isinstance(completed, list) or len(completed) != len(expected) \
            or set(completed) != expected:
        raise ValueError('manifest does not list every paired CSV')
    return manifest


def _rows(path, mode, seed, frames):
    with path.open(newline='') as source:
        reader = csv.DictReader(source)
        if not reader.fieldnames or None in reader.fieldnames:
            raise ValueError(f'invalid CSV header: {path}')
        fields = set(reader.fieldnames)
        base = {'policy', 'scenario', 'seed', 'elapsed_frames'}
        base |= ({'archetype', 'brain_generation',
                  'player_damage' if mode == 'maps' else 'damage_to_player',
                  'enemy_shots'} if mode != 'team' else {
                  'primary', 'ally', 'third', 'primary_generation',
                  'ally_generation', 'third_generation', 'protocol',
                  'player_damage', 'friendly_damage', 'friendly_hits',
                  'friendly_kills', 'enemy_shots', 'enemy_hits', 'fire_holds',
                  'robot_player_damage', 'helpers_player_damage'})
        if not base <= fields:
            raise ValueError(f'missing CSV columns {sorted(base - fields)}: {path}')
        role_fields = fields.intersection(ROLE_TELEMETRY)
        if role_fields and role_fields != set(ROLE_TELEMETRY):
            raise ValueError(f'incomplete role telemetry: {path}')
        expected = ({(role, scenario) for role in ROLES
                    for scenario in MAP_SCENARIOS + (
                        ('boss_cover',) if role == 'robot' else ())}
                    if mode == 'maps' else
                    {(role, scenario) for role in ROLES
                     for scenario in DUEL_SCENARIOS} if mode == 'duel' else
                    set(TEAM_SCENARIOS))
        result = {}
        for row in reader:
            if None in row or row.get('policy') not in ('scripted', 'neural'):
                raise ValueError(f'invalid CSV row: {path}')
            if _integer(row['seed'], path, 1) != seed:
                raise ValueError(f'seed mismatch: {path}')
            elapsed = _integer(row['elapsed_frames'], path, 1)
            if elapsed > frames:
                raise ValueError(f'frame limit exceeded: {path}')
            for key, value in row.items():
                if key not in TEXT_COLUMNS:
                    # Empty values occur only for the absent third participant.
                    if mode == 'team' and key == 'third_generation' and not row['third']:
                        if value != '0':
                            raise ValueError(f'invalid absent third generation: {path}')
                    else:
                        _number(value, f'{path}:{key}')
            if mode == 'team':
                scenario = row['scenario']
                participants = TEAM_SCENARIOS.get(scenario)
                if (participants is None or row['primary'] != participants[0]
                        or row['ally'] != participants[1]
                        or row['third'] != (participants[2]
                            if len(participants) == 3 else '')
                        or row['protocol'] != '2'):
                    raise ValueError(f'invalid team fixture: {path}')
                if abs(_number(row['robot_player_damage'], path)
                       + _number(row['helpers_player_damage'], path)
                       - _number(row['player_damage'], path)) > 0.02:
                    raise ValueError(f'team damage decomposition mismatch: {path}')
                if role_fields:
                    for metric, total in (('shots', 'enemy_shots'),
                                          ('player_hits', 'enemy_hits'),
                                          ('fire_holds', 'fire_holds')):
                        if sum(_integer(row[f'{position}_{metric}'], path)
                               for position in ('primary', 'ally', 'third')) \
                                != _integer(row[total], path):
                            raise ValueError(f'role telemetry mismatch: {path}')
                key = scenario
            else:
                key = (row['archetype'], row['scenario'])
            slot = (row['policy'], key)
            if key not in expected or slot in result:
                raise ValueError(f'unexpected or duplicate fixture: {path}')
            result[slot] = row
        for policy in ('scripted', 'neural'):
            if {key for candidate, key in result if candidate == policy} != expected:
                raise ValueError(f'incomplete {policy} fixtures: {path}')
        return result, bool(role_fields)


def _mean(values):
    return sum(values) / len(values)


def _ratio(active, bundle, smoothing):
    return (bundle + smoothing) / (active + smoothing)


def assess(directory):
    """Validate a frozen bundle and calculate a diagnostic, never eligibility."""
    directory = Path(directory)
    manifest = _manifest(directory)
    seeds, frames = manifest['seeds'], manifest['frames']
    rows, role_telemetry = {}, None
    for seed in seeds:
        for mode in ('maps', 'duel', 'team'):
            pair = {}
            flags = []
            for arm in ('active', 'bundle'):
                path = directory / f'{seed}-{mode}-{arm}.csv'
                pair[arm], has_telemetry = _rows(path, mode, seed, frames)
                flags.append(has_telemetry)
            if flags[0] != flags[1]:
                raise ValueError(f'role telemetry differs within pair: {seed} {mode}')
            if mode == 'team':
                if role_telemetry is not None and role_telemetry != flags[0]:
                    raise ValueError('team telemetry schema changed across seeds')
                role_telemetry = flags[0]
            scripted_active = {key: row for (policy, key), row in pair['active'].items()
                               if policy == 'scripted'}
            scripted_bundle = {key: row for (policy, key), row in pair['bundle'].items()
                               if policy == 'scripted'}
            if scripted_active != scripted_bundle:
                raise ValueError(f'scripted fixture changed within pair: {seed} {mode}')
            for arm in ('active', 'bundle'):
                rows[seed, mode, arm] = {key: row for (policy, key), row
                                         in pair[arm].items() if policy == 'neural'}

    generations = {'active': {}, 'bundle': {}}
    for seed in seeds:
        for mode in ('maps', 'duel', 'team'):
            for arm in ('active', 'bundle'):
                for key, row in rows[seed, mode, arm].items():
                    positions = ([(key[0], 'brain_generation')]
                        if mode != 'team' else
                        list(zip(TEAM_SCENARIOS[key],
                                 ('primary_generation', 'ally_generation',
                                  'third_generation'))))
                    for role, column in positions:
                        generation = _integer(row[column], f'{seed}:{mode}:{arm}:{role}', 1)
                        previous = generations[arm].setdefault(role, generation)
                        if previous != generation:
                            raise ValueError(f'brain generation changed: {arm} {role}')
    if any(set(generations[arm]) != set(ROLES) for arm in ('active', 'bundle')):
        raise ValueError('missing generation for one or more roles')

    roles = {}
    activity = []
    mode_ratios = {'maps': [], 'duel': []}
    for role in ROLES:
        roles[role] = {}
        for mode in ('maps', 'duel'):
            scenarios = MAP_SCENARIOS + (('boss_cover',) if role == 'robot' else ()) \
                if mode == 'maps' else DUEL_SCENARIOS
            field = 'player_damage' if mode == 'maps' else 'damage_to_player'
            per_scenario = {}
            for scenario in scenarios:
                values = {arm: [_number(rows[seed, mode, arm][role, scenario][field],
                                        f'{seed}:{mode}:{arm}:{role}:{scenario}')
                                for seed in seeds] for arm in ('active', 'bundle')}
                per_scenario[scenario] = {arm: _mean(samples)
                                          for arm, samples in values.items()}
                for index, seed in enumerate(seeds):
                    active, bundle = values['active'][index], values['bundle'][index]
                    candidate = rows[seed, mode, 'bundle'][role, scenario]
                    if (active >= 2 * HALF_HIT[role] and bundle == 0
                            and _integer(candidate['enemy_shots'], 'enemy_shots') == 0
                            and role != 'worker'):
                        activity.append({'seed': seed, 'mode': mode,
                                         'role': role, 'scenario': scenario,
                                         'active_damage': active,
                                         'bundle_damage': bundle,
                                         'bundle_shots': 0})
            active = _mean([entry['active'] for entry in per_scenario.values()])
            bundle = _mean([entry['bundle'] for entry in per_scenario.values()])
            ratio = _ratio(active, bundle, HALF_HIT[role])
            roles[role][mode] = {'active_damage': active,
                                 'bundle_damage': bundle, 'ratio': ratio,
                                 'scenarios': per_scenario}
            mode_ratios[mode].append(ratio)

    team = {}
    safety = []
    team_ratios = []
    helper_nonzero = False
    role_engagement = False
    for scenario, participants in TEAM_SCENARIOS.items():
        metrics = ('player_damage', 'friendly_damage', 'friendly_hits',
                   'friendly_kills', 'enemy_shots', 'enemy_hits', 'fire_holds',
                   'robot_player_damage', 'helpers_player_damage')
        team[scenario] = {}
        for arm in ('active', 'bundle'):
            team[scenario][arm] = {metric: _mean([
                _number(rows[seed, 'team', arm][scenario][metric],
                        f'{seed}:team:{arm}:{scenario}:{metric}') for seed in seeds])
                for metric in metrics}
        active = team[scenario]['active']['player_damage']
        bundle = team[scenario]['bundle']['player_damage']
        ratio = _ratio(active, bundle, HALF_HIT[participants[0]])
        team[scenario]['ratio'] = ratio
        team_ratios.append(ratio)
        for seed in seeds:
            a = rows[seed, 'team', 'active'][scenario]
            b = rows[seed, 'team', 'bundle'][scenario]
            if (_number(b['friendly_damage'], 'friendly_damage')
                    > _number(a['friendly_damage'], 'friendly_damage') + 0.001
                    or _integer(b['friendly_hits'], 'friendly_hits')
                    > _integer(a['friendly_hits'], 'friendly_hits')
                    or _integer(b['friendly_kills'], 'friendly_kills') > 0):
                safety.append({'seed': seed, 'scenario': scenario,
                    'active_friendly_damage': _number(a['friendly_damage'], 'friendly_damage'),
                    'bundle_friendly_damage': _number(b['friendly_damage'], 'friendly_damage'),
                    'active_friendly_hits': _integer(a['friendly_hits'], 'friendly_hits'),
                    'bundle_friendly_hits': _integer(b['friendly_hits'], 'friendly_hits'),
                    'bundle_friendly_kills': _integer(b['friendly_kills'], 'friendly_kills')})
            if scenario == 'boss_helpers' and _number(b['helpers_player_damage'],
                    'helpers_player_damage') > 0:
                helper_nonzero = True
            if role_telemetry:
                for position, role in zip(('primary', 'ally', 'third'), participants):
                    if (_integer(b[f'{position}_shots'], 'role shots')
                            + _integer(b[f'{position}_player_hits'], 'role hits') > 0
                            and role in ('worker', 'guard', 'officer', 'engineer')):
                        role_engagement = True

    components = {'team': _mean(team_ratios),
                  'maps': _mean(mode_ratios['maps']),
                  'duel': _mean(mode_ratios['duel'])}
    score = (0.55 * components['team'] + 0.30 * components['maps']
             + 0.15 * components['duel'])
    limitations = []
    if not role_telemetry:
        limitations.append('team CSV lacks nine per-role shots/hits/holds columns')
    if not helper_nonzero:
        limitations.append('boss_helpers has no helper damage to the player')
    if not role_engagement and role_telemetry:
        limitations.append('team fixtures show no non-Robot shots or hits')
    if len(seeds) == 2:
        limitations.append('quick stage only; four-seed wide evaluation required')
    if safety:
        limitations.append('friendly-fire regression in a paired fixture')
    if activity:
        limitations.append('no-action ranged fixture with active damage')
    decision = 'insufficient_evidence' if limitations else 'needs_playtest'
    return {'decision': decision, 'provisional_macro_score': score,
            'weights': {'team': 0.55, 'maps': 0.30, 'duel': 0.15},
            'score_components': components,
            'protocol': {'seeds': seeds, 'frames': frames,
                         'stage': manifest['stage'],
                         'evaluator_source_revision': manifest['evaluator_source_revision'],
                         'evaluator_sha256': manifest['evaluator_sha256'],
                         'active_brain_sha256': manifest['active_brain_sha256'],
                         'bundle_brain_sha256': {role: manifest['candidate_brains'][role]['sha256']
                                                 for role in ROLES},
                         'generations': generations},
            'roles': roles, 'team_scenarios': team,
            'safety_failures': safety, 'activity_failures': activity,
            'limitations': limitations}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    try:
        report = assess(args.directory)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        parser.exit(2, f'assess_bundle: {error}\n')
    print(json.dumps(report, indent=2, sort_keys=True, allow_nan=False))


if __name__ == '__main__':
    main()
