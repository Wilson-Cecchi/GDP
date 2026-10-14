import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/zimaos'))
from training_evaluation import summarize, summarize_team, archive_candidate


def reports(root, center_candidate=12):
    for seed in (7, 1001):
        for mode in ('maps', 'duel'):
            for policy in ('active', 'candidate'):
                scenarios = ('open', 'center_cover', 'elevated', 'group') if mode == 'maps' else ('ground_strafe', 'bhop')
                damage_key = 'player_damage' if mode == 'maps' else 'damage_to_player'
                with (root / f'{seed}-{mode}-{policy}.csv').open('w') as out:
                    writer = csv.DictWriter(out, fieldnames=['policy','scenario','archetype','seed',
                        'brain_generation','elapsed_frames',damage_key,'enemy_travel','outcome'])
                    writer.writeheader()
                    for scenario in scenarios:
                        damage = 12 if scenario == 'center_cover' else 20
                        if policy == 'candidate':
                            damage = center_candidate if scenario == 'center_cover' else 40
                        writer.writerow({'policy':'neural','scenario':scenario,'archetype':'guard',
                            'seed':seed,'brain_generation':200 if policy == 'active' else 910,
                            'elapsed_frames':360,damage_key:damage,'enemy_travel':3,'outcome':'player_win'})


class EvaluationTests(unittest.TestCase):
    def test_v2_boss_requires_robot_contribution_and_complete_telemetry(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fields = ['policy', 'scenario', 'seed', 'primary', 'ally',
                      'primary_generation', 'ally_generation', 'elapsed_frames',
                      'player_damage', 'friendly_damage', 'friendly_hits',
                      'friendly_kills', 'protocol', 'third', 'third_generation',
                      'enemy_shots', 'enemy_hits', 'fire_holds',
                      'robot_player_damage', 'helpers_player_damage',
                      'enemies_alive', 'boss_health_remaining',
                      'first_player_hit_frame', 'boss_distance_initial',
                      'boss_distance_minimum', 'boss_distance_final',
                      'boss_long_range', 'boss_pressure', 'boss_pursuit']
            pairs = [('firing_lane', 'guard', 'worker', ''),
                     ('officer_guard', 'officer', 'guard', ''),
                     ('engineer_guard', 'engineer', 'guard', ''),
                     ('boss_helpers', 'robot', 'officer', 'guard')]
            for policy in ('active', 'candidate'):
                path = root / f'150001-team-{policy}.csv'
                with path.open('w') as out:
                    writer = csv.DictWriter(out, fieldnames=fields)
                    writer.writeheader()
                    for scenario, primary, ally, third in pairs:
                        robot_damage = (20 if policy == 'active' else 5) \
                            if scenario == 'boss_helpers' else 0
                        helper_damage = (20 if policy == 'active' else 35) \
                            if scenario == 'boss_helpers' else 20
                        writer.writerow({'policy': 'neural', 'scenario': scenario,
                            'seed': 150001, 'primary': primary, 'ally': ally,
                            'primary_generation': 910 if policy == 'candidate'
                                and primary == 'robot' else 200,
                            'ally_generation': 200, 'third_generation': 200,
                            'elapsed_frames': 480, 'player_damage':
                                robot_damage + helper_damage,
                            'friendly_damage': 0, 'friendly_hits': 0,
                            'friendly_kills': 0, 'protocol': 2, 'third': third,
                            'enemy_shots': 4, 'enemy_hits': 2, 'fire_holds': 0,
                            'robot_player_damage': robot_damage,
                            'helpers_player_damage': helper_damage,
                            'enemies_alive': 1, 'boss_health_remaining': 20,
                            'first_player_hit_frame': 80,
                            'boss_distance_initial': 12,
                            'boss_distance_minimum': 10,
                            'boss_distance_final': 11,
                            'boss_long_range': 200,
                            'boss_pressure': 150, 'boss_pursuit': 130})
            report = summarize_team(root, 'robot', (150001,), 480,
                                    'revision', protocol_version=2)
            self.assertFalse(report['eligible'])
            self.assertEqual('robot_player_damage', report['regressions'][0]['metric'])
            path = root / '150001-team-candidate.csv'
            lines = path.read_text().splitlines()
            lines[-1] = lines[-1].replace(',2,guard,200,', ',1,guard,200,')
            path.write_text('\n'.join(lines) + '\n')
            with self.assertRaises(ValueError):
                summarize_team(root, 'robot', (150001,), 480,
                               'revision', protocol_version=2)

    def test_team_friendly_fire_blocks_high_damage_candidate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fields = ['policy', 'scenario', 'seed', 'primary', 'ally',
                      'primary_generation', 'ally_generation', 'elapsed_frames',
                      'player_damage', 'friendly_damage', 'friendly_hits',
                      'friendly_kills']
            pairs = [('firing_lane', 'guard', 'worker'),
                     ('officer_guard', 'officer', 'guard'),
                     ('engineer_guard', 'engineer', 'guard')]
            for seed in (9001, 9002):
                for policy in ('active', 'candidate'):
                    with (root / f'{seed}-team-{policy}.csv').open('w') as out:
                        writer = csv.DictWriter(out, fieldnames=fields)
                        writer.writeheader()
                        for scenario, primary, ally in pairs:
                            writer.writerow({'policy': 'neural', 'scenario': scenario,
                                'seed': seed, 'primary': primary, 'ally': ally,
                                'primary_generation': 2 if policy == 'candidate' and primary == 'guard' else 200,
                                'ally_generation': 2 if policy == 'candidate' and ally == 'guard' else 200,
                                'elapsed_frames': 480,
                                'player_damage': 40 if policy == 'candidate' else 20,
                                'friendly_damage': 16 if policy == 'candidate' else 8,
                                'friendly_hits': 2 if policy == 'candidate' else 1,
                                'friendly_kills': 0})
            report = summarize_team(root, 'guard', (9001, 9002), 480, 'revision')
            self.assertGreater(report['score'], 1)
            self.assertFalse(report['eligible'])
            self.assertEqual(6, len(report['regressions']))
            self.assertTrue(all(row['metric'] == 'friendly_damage'
                                for row in report['regressions']))
            with self.assertRaises(ValueError):
                summarize_team(root, 'robot', (9001, 9002), 480, 'revision')

    def test_mean_gain_cannot_hide_cover_regression(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); reports(root, center_candidate=4)
            result = summarize(root, 'guard', (7,1001), 360, 'revision')
            self.assertGreater(result['score'], 1)
            self.assertFalse(result['eligible'])
            self.assertEqual(2, len(result['regressions']))
            self.assertTrue(all(r['scenario']=='center_cover' for r in result['regressions']))

    def test_partial_or_nonfinite_reports_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); reports(root)
            path = root / '7-maps-candidate.csv'
            original = path.read_text()
            path.write_text('\n'.join(original.splitlines()[:-1])+'\n')
            with self.assertRaises(ValueError): summarize(root,'guard',(7,1001),360,'revision')
            path.write_text(original.replace(',40,3,',',nan,3,'))
            with self.assertRaises(ValueError): summarize(root,'guard',(7,1001),360,'revision')

    def test_archive_keeps_best_and_separates_protocols(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary); evaluation = root / 'evaluation'; evaluation.mkdir(); reports(evaluation)
            report = summarize(evaluation,'guard',(7,1001),360,'revision')
            champion = root / 'batch-one' / 'guard_generation_000910.brain'
            champion.parent.mkdir(); champion.write_text('preserved champion')
            self.assertTrue(archive_candidate(report,champion,root,'quick'))
            lower = {**report, 'score':report['score']-0.1}
            self.assertFalse(archive_candidate(lower,champion,root,'quick'))
            bests = list((root/'candidates').rglob('best.json'))
            self.assertEqual(report['score'],json.loads(bests[0].read_text())['score'])
            changed = {**report, 'protocol':{**report['protocol'],'frames':1200}}
            self.assertTrue(archive_candidate(changed,champion,root,'quick'))
            self.assertEqual(2,len(list((root/'candidates').rglob('best.json'))))
