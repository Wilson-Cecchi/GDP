import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/zimaos'))
from rank_checkpoints import load_evaluation, rank_checkpoints
from training_evaluation import (TEAM_PARTICIPANTS_V2, combine_new_mechanics,
                                 summarize, summarize_team)


SEEDS = (150001, 150011)
FRAMES = 480
REVISION = 'same-binary-source'
TEAM_FIELDS = ('policy scenario seed primary ally primary_generation '
               'ally_generation elapsed_frames player_damage friendly_damage '
               'friendly_hits friendly_kills protocol third third_generation '
               'enemy_shots enemy_hits fire_holds robot_player_damage '
               'helpers_player_damage enemies_alive boss_health_remaining '
               'first_player_hit_frame boss_distance_initial '
               'boss_distance_minimum boss_distance_final boss_long_range '
               'boss_pressure boss_pursuit').split()


def make_report(directory, role, generation, *, cover_regression=False,
                revision=REVISION):
    directory.mkdir()
    for seed in SEEDS:
        for mode in ('maps', 'duel'):
            scenarios = ['open', 'center_cover', 'elevated', 'group'] if mode == 'maps' \
                else ['ground_strafe', 'bhop']
            if role == 'robot' and mode == 'maps':
                scenarios.append('boss_cover')
            metric = 'player_damage' if mode == 'maps' else 'damage_to_player'
            fields = ['policy', 'scenario', 'archetype', 'seed',
                      'brain_generation', 'elapsed_frames', metric,
                      'enemy_travel', 'outcome']
            for policy in ('active', 'candidate'):
                with (directory / f'{seed}-{mode}-{policy}.csv').open('w') as out:
                    writer = csv.DictWriter(out, fieldnames=fields)
                    writer.writeheader()
                    for scenario in scenarios:
                        damage = 20 if policy == 'active' else 25
                        if cover_regression and policy == 'candidate':
                            damage = 0 if scenario == 'center_cover' else 60
                        writer.writerow({'policy': 'neural', 'scenario': scenario,
                                         'archetype': role, 'seed': seed,
                                         'brain_generation': (200 if policy == 'active'
                                                              else generation),
                                         'elapsed_frames': FRAMES, metric: damage,
                                         'enemy_travel': 3, 'outcome': 'player_win'})
        for policy in ('active', 'candidate'):
            with (directory / f'{seed}-team-{policy}.csv').open('w') as out:
                writer = csv.DictWriter(out, fieldnames=TEAM_FIELDS)
                writer.writeheader()
                for scenario, participants in TEAM_PARTICIPANTS_V2.items():
                    members = list(participants) + [''] * (3 - len(participants))
                    generations = [generation if policy == 'candidate'
                                   and member == role else 200 for member in members]
                    robot_damage = 10 if scenario == 'boss_helpers' else 0
                    player_damage = (25 if policy == 'candidate' and role in participants
                                     else 20)
                    if scenario == 'boss_helpers' and role == 'robot' and policy == 'candidate':
                        robot_damage = 15
                    writer.writerow({'policy': 'neural', 'scenario': scenario,
                                     'seed': seed, 'primary': members[0],
                                     'ally': members[1], 'third': members[2],
                                     'primary_generation': generations[0],
                                     'ally_generation': generations[1],
                                     'third_generation': generations[2] if members[2] else 0,
                                     'elapsed_frames': FRAMES,
                                     'player_damage': player_damage,
                                     'friendly_damage': 0, 'friendly_hits': 0,
                                     'friendly_kills': 0, 'protocol': 2,
                                     'enemy_shots': 5, 'enemy_hits': 2,
                                     'fire_holds': 1,
                                     'robot_player_damage': robot_damage,
                                     'helpers_player_damage': player_damage - robot_damage,
                                     'enemies_alive': 1, 'boss_health_remaining': 10,
                                     'first_player_hit_frame': 80,
                                     'boss_distance_initial': 20,
                                     'boss_distance_minimum': 10,
                                     'boss_distance_final': 15,
                                     'boss_long_range': 100,
                                     'boss_pressure': 100, 'boss_pursuit': 50})
    report = combine_new_mechanics(
        summarize(directory, role, SEEDS, FRAMES, revision),
        summarize_team(directory, role, SEEDS, FRAMES, revision, protocol_version=2))
    path = directory / 'report.json'
    path.write_text(json.dumps(report))
    return path


class RankCheckpointTests(unittest.TestCase):
    def test_eligible_precedes_high_score_with_cover_regression(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            eligible = make_report(root / 'eligible', 'guard', 70)
            high_score = make_report(root / 'regressed', 'guard', 140,
                                     cover_regression=True)
            result = rank_checkpoints([high_score, eligible])
            self.assertEqual([70, 140], [row['generation']
                                         for row in result['ranking']])
            self.assertTrue(result['ranking'][0]['eligible'])
            self.assertGreater(result['ranking'][1]['score'],
                               result['ranking'][0]['score'])
            self.assertGreater(result['ranking'][1]['regression_count'], 0)
            self.assertLess(result['ranking'][1]['scenarios']
                            ['solo:maps:center_cover']['player_damage_delta'], 0)
            self.assertEqual({'guard': 200, 'worker': 200, 'officer': 200,
                              'engineer': 200, 'robot': 200},
                             result['baseline_generations'])

    def test_robot_diagnostics_include_own_damage_and_boss_motion(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = make_report(Path(temporary) / 'robot', 'robot', 70)
            row = rank_checkpoints([path])['ranking'][0]
            self.assertTrue(row['eligible'])
            boss = row['scenarios']['team:boss_helpers']
            self.assertEqual(5, boss['robot_player_damage_delta'])
            self.assertEqual(0, boss['boss_pressure_delta'])

    def test_rejects_different_protocol_and_allies(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            first = make_report(root / 'first', 'guard', 70)
            other_revision = make_report(root / 'revision', 'guard', 140,
                                         revision='different-source')
            with self.assertRaisesRegex(ValueError, 'different role, protocol'):
                rank_checkpoints([first, other_revision])
            second = make_report(root / 'second', 'guard', 140)
            path = second.parent / '150001-team-candidate.csv'
            with path.open(newline='') as source:
                rows = list(csv.DictReader(source))
            rows[0]['ally_generation'] = '201'
            with path.open('w', newline='') as destination:
                writer = csv.DictWriter(destination, fieldnames=TEAM_FIELDS)
                writer.writeheader()
                writer.writerows(rows)
            with self.assertRaisesRegex(ValueError, 'active or ally generation changed'):
                rank_checkpoints([first, second])

    def test_rejects_changed_report_invalid_csv_and_reserved_seed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            path = make_report(root / 'valid', 'guard', 70)
            report = json.loads(path.read_text())
            report['score'] = 999
            path.write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError, 'differs from paired CSV'):
                load_evaluation(path)
            report['score'] = 1.0
            report['protocol']['seeds'] = [151001]
            path.write_text(json.dumps(report))
            with self.assertRaisesRegex(ValueError, 'reserved evaluation protocol'):
                load_evaluation(path)
            path = make_report(root / 'invalid', 'guard', 140)
            csv_path = path.parent / '150001-maps-candidate.csv'
            csv_path.write_text(csv_path.read_text().replace(',25,3,', ',nan,3,'))
            with self.assertRaisesRegex(ValueError, 'invalid metric'):
                load_evaluation(path)


if __name__ == '__main__':
    unittest.main()
