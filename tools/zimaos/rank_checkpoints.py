"""Read-only ranking of comparable new-mechanics checkpoint evaluations.

The original CSVs are required: report.json alone does not record the active
and allied brain generations, and cannot establish comparable opponents.
"""

import argparse
import csv
import json
import math
from pathlib import Path

from training_evaluation import (TEAM_PARTICIPANTS_V2, combine_new_mechanics,
                                 summarize, summarize_team)


GATE = 'paired-solo-and-team-new-mechanics-v2'
FINAL_SEEDS = {151001, 151013, 151027, 151039,
               151051, 151061, 151073, 151099}
GENERATION_COLUMNS = ('primary_generation', 'ally_generation',
                      'third_generation')


def _generation(value, path):
    try:
        result = int(value)
    except (TypeError, ValueError) as error:
        raise ValueError(f'invalid brain generation: {path}') from error
    if result < 0:
        raise ValueError(f'invalid brain generation: {path}')
    return result


def _identity(directory, role, seeds, candidate_generation):
    """Extract active and ally generations, checking each paired CSV row."""
    baseline = {}
    for seed in seeds:
        for mode in ('maps', 'duel'):
            for policy in ('active', 'candidate'):
                path = directory / f'{seed}-{mode}-{policy}.csv'
                with path.open(newline='') as source:
                    rows = [row for row in csv.DictReader(source)
                            if row.get('policy') == 'neural'
                            and row.get('archetype') == role]
                for row in rows:
                    generation = _generation(row.get('brain_generation'), path)
                    expected = candidate_generation if policy == 'candidate' else None
                    if expected is not None and generation != expected:
                        raise ValueError(f'candidate generation mismatch: {path}')
                    if policy == 'active':
                        previous = baseline.setdefault(role, generation)
                        if previous != generation:
                            raise ValueError(f'active generation changed: {path}')
        for policy in ('active', 'candidate'):
            path = directory / f'{seed}-team-{policy}.csv'
            with path.open(newline='') as source:
                rows = [row for row in csv.DictReader(source)
                        if row.get('policy') == 'neural']
            for row in rows:
                scenario = row.get('scenario')
                if scenario not in TEAM_PARTICIPANTS_V2:
                    raise ValueError(f'unknown team scenario: {path}')
                participants = TEAM_PARTICIPANTS_V2[scenario]
                if role not in participants:
                    continue
                for member, column in zip(participants, GENERATION_COLUMNS):
                    generation = _generation(row.get(column), path)
                    if policy == 'candidate' and member == role:
                        if generation != candidate_generation:
                            raise ValueError(f'candidate generation mismatch: {path}')
                        continue
                    previous = baseline.setdefault(member, generation)
                    if previous != generation:
                        raise ValueError(f'active or ally generation changed: {path}')
    if role not in baseline:
        raise ValueError(f'missing active generation for {role}')
    return dict(sorted(baseline.items()))


def _diagnostics(report):
    scenarios = {}
    for mode in ('solo', 'team'):
        for name, policies in sorted(report['scenarios'][mode].items()):
            active = policies['active']
            candidate = policies['candidate']
            metric = 'damage' if mode == 'solo' else 'player_damage'
            entry = {'player_damage_delta': candidate[metric] - active[metric]}
            if mode == 'team':
                for key in ('friendly_damage', 'friendly_hits', 'friendly_kills',
                            'enemy_shots', 'enemy_hits', 'fire_holds',
                            'robot_player_damage', 'helpers_player_damage',
                            'boss_pressure', 'boss_pursuit'):
                    if key in active and key in candidate:
                        entry[f'{key}_delta'] = candidate[key] - active[key]
            scenarios[f'{mode}:{name}'] = entry
    regressions = sorted(report['regressions'], key=lambda row: (
        row.get('mode', ''), row.get('scenario', ''), row.get('seed', -1),
        row.get('metric', 'player_damage')))
    return {'scenarios': scenarios, 'regressions': regressions}


def _same_evidence(actual, expected):
    """Allow only floating-point summation noise in archived JSON reports."""
    if isinstance(expected, dict):
        return (isinstance(actual, dict) and actual.keys() == expected.keys()
                and all(_same_evidence(actual[key], value)
                        for key, value in expected.items()))
    if isinstance(expected, list):
        return (isinstance(actual, list) and len(actual) == len(expected)
                and all(_same_evidence(left, right)
                        for left, right in zip(actual, expected)))
    if type(expected) is float:
        return (type(actual) in (int, float) and math.isfinite(actual)
                and math.isclose(actual, expected, rel_tol=1e-12,
                                 abs_tol=1e-9))
    return type(actual) is type(expected) and actual == expected


def load_evaluation(path):
    """Recompute a report, then record its active/allied comparison identity."""
    path = Path(path)
    report = json.loads(path.read_text())
    if not isinstance(report, dict) or not isinstance(report.get('protocol'), dict):
        raise ValueError(f'invalid report: {path}')
    protocol = report['protocol']
    if set(protocol) != {'seeds', 'frames', 'source_revision', 'gate'} or protocol['gate'] != GATE:
        raise ValueError(f'unsupported evaluation protocol: {path}')
    seeds, frames, revision = (protocol['seeds'], protocol['frames'],
                               protocol['source_revision'])
    if (not isinstance(seeds, list) or not seeds
            or any(type(seed) is not int or seed < 0 for seed in seeds)
            or len(set(seeds)) != len(seeds)
            or set(seeds) & FINAL_SEEDS
            or type(frames) is not int or frames < 1
            or not isinstance(revision, str) or not revision.strip()):
        raise ValueError(f'invalid or reserved evaluation protocol: {path}')
    role = report.get('role')
    if role not in ('worker', 'guard', 'officer', 'engineer', 'robot'):
        raise ValueError(f'invalid role: {path}')
    directory = path.parent
    expected = combine_new_mechanics(
        summarize(directory, role, seeds, frames, revision),
        summarize_team(directory, role, seeds, frames, revision,
                       protocol_version=2))
    if not _same_evidence(report, expected):
        raise ValueError(f'report differs from paired CSV evidence: {path}')
    baseline = _identity(directory, role, seeds, report['generation'])
    return {'path': str(path), 'report': report, 'baseline_generations': baseline,
            'diagnostics': _diagnostics(report)}


def rank_checkpoints(paths):
    """Rank one role and one exact protocol; never read reserved final seeds."""
    paths = [Path(path) for path in paths]
    if len(paths) != len(set(paths)) or not paths:
        raise ValueError('provide distinct report paths')
    evaluations = [load_evaluation(path) for path in paths]
    reference = evaluations[0]
    for item in evaluations[1:]:
        if (item['report']['role'] != reference['report']['role']
                or item['report']['protocol'] != reference['report']['protocol']
                or item['baseline_generations'] != reference['baseline_generations']):
            raise ValueError('reports have different role, protocol, active brain or allies')
    evaluations.sort(key=lambda item: (
        not item['report']['eligible'],
        len(item['report']['regressions']),
        -item['report']['score'],
        item['report']['generation'], item['path']))
    return {'role': reference['report']['role'],
            'protocol': reference['report']['protocol'],
            'binary_provenance': 'source revision only; verify executable SHA-256 in an external manifest',
            'baseline_generations': reference['baseline_generations'],
            'ranking': [{'rank': index, 'path': item['path'],
                         'generation': item['report']['generation'],
                         'eligible': item['report']['eligible'],
                         'score': item['report']['score'],
                         'regression_count': len(item['report']['regressions']),
                         **item['diagnostics']}
                        for index, item in enumerate(evaluations, 1)]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reports', nargs='+', type=Path,
                        help='report.json paths with their paired CSVs')
    args = parser.parse_args()
    print(json.dumps(rank_checkpoints(args.reports), indent=2, sort_keys=True))


if __name__ == '__main__':
    main()
