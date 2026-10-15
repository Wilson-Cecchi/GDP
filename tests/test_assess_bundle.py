import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/zimaos'))
from assess_bundle import (HALF_HIT, MAP_SCENARIOS, ROLES, ROLE_TELEMETRY,
                           TEAM_SCENARIOS, assess)


MAP_FIELDS = ('policy', 'scenario', 'archetype', 'seed', 'brain_generation',
              'elapsed_frames', 'player_damage', 'enemy_shots')
DUEL_FIELDS = ('policy', 'scenario', 'archetype', 'seed', 'brain_generation',
               'elapsed_frames', 'damage_to_player', 'enemy_shots')
TEAM_FIELDS = ('policy', 'scenario', 'seed', 'primary', 'ally', 'third',
               'primary_generation', 'ally_generation', 'third_generation',
               'elapsed_frames', 'protocol', 'player_damage', 'friendly_damage',
               'friendly_hits', 'friendly_kills', 'enemy_shots', 'enemy_hits',
               'fire_holds', 'robot_player_damage', 'helpers_player_damage')


def write_csv(path, fields, rows):
    with path.open('w', newline='') as source:
        writer = csv.DictWriter(source, fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)


def change_row(path, predicate, updates):
    with path.open(newline='') as source:
        reader = csv.DictReader(source)
        fields = reader.fieldnames
        rows = list(reader)
    for row in rows:
        if predicate(row):
            row.update(updates)
    write_csv(path, fields, rows)


def make_bundle(root, *, stage='wide', telemetry=True, map_damage=None):
    seeds = [150001, 150011] if stage == 'quick' else [
        150001, 150011, 150031, 150053]
    completed = []
    generations = dict(zip(ROLES, (110, 120, 130, 140, 150)))
    for seed in seeds:
        for mode in ('maps', 'duel', 'team'):
            for arm in ('active', 'bundle'):
                path = root / f'{seed}-{mode}-{arm}.csv'
                completed.append(path.name)
                rows = []
                if mode in ('maps', 'duel'):
                    fields = MAP_FIELDS if mode == 'maps' else DUEL_FIELDS
                    damage_field = 'player_damage' if mode == 'maps' else 'damage_to_player'
                    for role in ROLES:
                        scenarios = (MAP_SCENARIOS + (('boss_cover',) if role == 'robot' else ())
                                     if mode == 'maps' else ('ground_strafe', 'bhop'))
                        for scenario in scenarios:
                            for policy in ('scripted', 'neural'):
                                damage = (20 if arm == 'active' or policy == 'scripted'
                                          else (map_damage or {}).get(role, 30)
                                          if mode == 'maps' else 30)
                                rows.append({'policy': policy, 'scenario': scenario,
                                    'archetype': role, 'seed': seed,
                                    'brain_generation': 0 if policy == 'scripted'
                                        else 200 if arm == 'active' else generations[role],
                                    'elapsed_frames': 100, damage_field: damage,
                                    'enemy_shots': 3})
                else:
                    fields = TEAM_FIELDS + (ROLE_TELEMETRY if telemetry else ())
                    for scenario, participants in TEAM_SCENARIOS.items():
                        for policy in ('scripted', 'neural'):
                            damage = 20 if arm == 'active' or policy == 'scripted' else 30
                            row = {'policy': policy, 'scenario': scenario,
                                'seed': seed, 'primary': participants[0],
                                'ally': participants[1],
                                'third': participants[2] if len(participants) == 3 else '',
                                'primary_generation': 0 if policy == 'scripted'
                                    else 200 if arm == 'active' else generations[participants[0]],
                                'ally_generation': 0 if policy == 'scripted'
                                    else 200 if arm == 'active' else generations[participants[1]],
                                'third_generation': 0 if len(participants) == 2
                                    or policy == 'scripted' else 200 if arm == 'active'
                                    else generations[participants[2]],
                                'elapsed_frames': 100, 'protocol': 2,
                                'player_damage': damage, 'friendly_damage': 0,
                                'friendly_hits': 0, 'friendly_kills': 0,
                                'enemy_shots': 4, 'enemy_hits': 2,
                                'fire_holds': 0,
                                'robot_player_damage': damage / 2
                                    if scenario == 'boss_helpers' else 0,
                                'helpers_player_damage': damage / 2
                                    if scenario == 'boss_helpers' else damage}
                            if telemetry:
                                row.update({'primary_shots': 2, 'ally_shots': 1,
                                    'third_shots': 1, 'primary_player_hits': 1,
                                    'ally_player_hits': 1, 'third_player_hits': 0,
                                    'primary_fire_holds': 0, 'ally_fire_holds': 0,
                                    'third_fire_holds': 0})
                            rows.append(row)
                write_csv(path, fields, rows)
    manifest = {'evaluator_source_revision': 'revision',
        'evaluator_sha256': 'a' * 64,
        'active_brain_sha256': {role: 'b' * 64 for role in ROLES},
        'candidate_brains': {role: {'path': f'/data/{role}.brain',
                                    'sha256': 'c' * 64} for role in ROLES},
        'stage': stage, 'seeds': seeds, 'frames': 1200 if stage == 'wide' else 480,
        'modes': ['maps', 'duel', 'team'], 'completed': completed, 'complete': True}
    (root / 'manifest.json').write_text(json.dumps(manifest))
    return seeds


class AssessBundleTests(unittest.TestCase):
    def test_complete_bundle_macro_score_and_review_decision(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            make_bundle(root, map_damage={'guard': 50})
            report = assess(root)
            self.assertEqual('needs_playtest', report['decision'])
            self.assertEqual([], report['limitations'])
            self.assertEqual([], report['safety_failures'])
            self.assertEqual([], report['activity_failures'])
            expected_team = sum((30 + HALF_HIT[TEAM_SCENARIOS[name][0]])
                / (20 + HALF_HIT[TEAM_SCENARIOS[name][0]])
                for name in TEAM_SCENARIOS) / len(TEAM_SCENARIOS)
            expected_maps = sum(((50 if role == 'guard' else 30) + HALF_HIT[role])
                / (20 + HALF_HIT[role]) for role in ROLES) / len(ROLES)
            expected_duel = sum((30 + HALF_HIT[role]) / (20 + HALF_HIT[role])
                for role in ROLES) / len(ROLES)
            self.assertAlmostEqual(expected_team, report['score_components']['team'])
            self.assertAlmostEqual(expected_maps, report['score_components']['maps'])
            self.assertAlmostEqual(0.55 * expected_team + 0.30 * expected_maps
                                   + 0.15 * expected_duel,
                                   report['provisional_macro_score'])
            self.assertEqual(120, report['protocol']['generations']['bundle']['guard'])

    def test_friendly_fire_is_reported_even_with_better_damage(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            make_bundle(root)
            path = root / '150001-team-bundle.csv'
            change_row(path, lambda row: row['policy'] == 'neural'
                and row['scenario'] == 'firing_lane',
                {'friendly_damage': 8, 'friendly_hits': 1, 'friendly_kills': 1})
            report = assess(root)
            self.assertEqual('insufficient_evidence', report['decision'])
            self.assertEqual(1, len(report['safety_failures']))

    def test_no_shot_is_failure_but_hold_is_not(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            make_bundle(root)
            path = root / '150001-maps-bundle.csv'
            change_row(path, lambda row: row['policy'] == 'neural'
                and row['archetype'] == 'robot' and row['scenario'] == 'boss_cover',
                {'player_damage': 0, 'enemy_shots': 0})
            report = assess(root)
            self.assertEqual('insufficient_evidence', report['decision'])
            self.assertEqual('boss_cover', report['activity_failures'][0]['scenario'])
            self.assertEqual(1, len(report['activity_failures']))
            path = root / '150001-team-bundle.csv'
            change_row(path, lambda row: row['policy'] == 'neural'
                and row['scenario'] == 'firing_lane',
                {'primary_shots': 0, 'ally_shots': 0, 'third_shots': 0,
                 'enemy_shots': 0, 'primary_fire_holds': 4,
                 'fire_holds': 4, 'player_damage': 0,
                 'helpers_player_damage': 0,
                 'primary_player_hits': 0, 'ally_player_hits': 0,
                 'enemy_hits': 0})
            report = assess(root)
            self.assertEqual(1, len(report['activity_failures']))

    def test_old_team_csv_is_parsed_with_limitations(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            make_bundle(root, stage='quick', telemetry=False)
            report = assess(root)
            self.assertEqual('insufficient_evidence', report['decision'])
            self.assertTrue(any('nine per-role' in item for item in report['limitations']))
            self.assertTrue(any('quick stage' in item for item in report['limitations']))

    def test_incomplete_or_mismatched_evidence_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            make_bundle(root)
            path = root / '150001-maps-bundle.csv'
            path.unlink()
            with self.assertRaises(FileNotFoundError):
                assess(root)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            make_bundle(root)
            path = root / '150001-team-bundle.csv'
            change_row(path, lambda row: row['policy'] == 'neural'
                and row['scenario'] == 'boss_helpers',
                {'primary_generation': 999})
            with self.assertRaisesRegex(ValueError, 'generation changed'):
                assess(root)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            make_bundle(root)
            path = root / '150001-duel-bundle.csv'
            change_row(path, lambda row: row['policy'] == 'neural'
                and row['archetype'] == 'robot', {'damage_to_player': 'nan'})
            with self.assertRaisesRegex(ValueError, 'invalid number'):
                assess(root)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            make_bundle(root)
            path = root / '150001-maps-bundle.csv'
            with path.open(newline='') as source:
                reader = csv.DictReader(source)
                fields, rows = reader.fieldnames, list(reader)
            rows = [row for row in rows if not (row['policy'] == 'neural'
                and row['archetype'] == 'robot' and row['scenario'] == 'boss_cover')]
            write_csv(path, fields, rows)
            with self.assertRaisesRegex(ValueError, 'incomplete neural'):
                assess(root)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            make_bundle(root)
            manifest_path = root / 'manifest.json'
            manifest = json.loads(manifest_path.read_text())
            manifest['seeds'][0] = 151001
            manifest_path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'reserved'):
                assess(root)
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            make_bundle(root)
            manifest_path = root / 'manifest.json'
            manifest = json.loads(manifest_path.read_text())
            manifest['frames'] = 480
            manifest_path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError, 'stage or frames'):
                assess(root)


if __name__ == '__main__':
    unittest.main()
