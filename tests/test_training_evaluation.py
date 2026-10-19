import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/zimaos'))
from training_evaluation import (summarize, summarize_team, summarize_investigation,
                                 archive_candidate)


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


def attributed_team_reports(root, active_fire=None, candidate_fire=None):
    """Write complete v3 fixtures; fire maps shooter slot to (hits, kills, damage)."""
    fields = ['policy', 'scenario', 'seed', 'primary', 'ally', 'third',
              'primary_generation', 'ally_generation', 'third_generation',
              'elapsed_frames', 'player_damage', 'friendly_damage',
              'friendly_hits', 'friendly_kills', 'protocol', 'enemy_shots',
              'enemy_hits', 'fire_holds', 'robot_player_damage',
              'helpers_player_damage', 'enemies_alive', 'boss_health_remaining',
              'first_player_hit_frame', 'boss_distance_initial',
              'boss_distance_minimum', 'boss_distance_final', 'boss_long_range',
              'boss_pressure', 'boss_pursuit']
    fields += [f'{slot}_friendly_{metric}'
               for metric in ('hits', 'kills', 'damage')
               for slot in ('primary', 'ally', 'third')]
    pairs = [('firing_lane', 'guard', 'worker', ''),
             ('officer_guard', 'officer', 'guard', ''),
             ('engineer_guard', 'engineer', 'guard', ''),
             ('boss_helpers', 'robot', 'officer', 'guard')]
    for policy, fire in (('active', active_fire or {}),
                         ('candidate', candidate_fire or {})):
        with (root / f'152239-team-{policy}.csv').open('w') as out:
            writer = csv.DictWriter(out, fieldnames=fields)
            writer.writeheader()
            for scenario, primary, ally, third in pairs:
                row = dict(policy='neural', scenario=scenario, seed=152239,
                           primary=primary, ally=ally, third=third,
                           primary_generation=200,
                           ally_generation=280 if policy == 'candidate'
                               and scenario == 'firing_lane' else 200,
                           third_generation=200, elapsed_frames=480,
                           player_damage=42 if policy == 'candidate' else 32,
                           friendly_damage=0, friendly_hits=0, friendly_kills=0,
                           protocol=3, enemy_shots=8, enemy_hits=5,
                           fire_holds=68, robot_player_damage=20
                               if scenario == 'boss_helpers' else 0,
                           helpers_player_damage=(22 if policy == 'candidate' else 12)
                               if scenario == 'boss_helpers'
                               else (42 if policy == 'candidate' else 32),
                           enemies_alive=1, boss_health_remaining=20,
                           first_player_hit_frame=80, boss_distance_initial=12,
                           boss_distance_minimum=10, boss_distance_final=11,
                           boss_long_range=200, boss_pressure=150,
                           boss_pursuit=130)
                for metric in ('hits', 'kills', 'damage'):
                    row[f'friendly_{metric}'] = sum(
                        fire.get(slot, (0, 0, 0))[('hits', 'kills', 'damage').index(metric)]
                        for slot in ('primary', 'ally', 'third')) if scenario == 'firing_lane' else 0
                    for slot in ('primary', 'ally', 'third'):
                        row[f'{slot}_friendly_{metric}'] = (
                            fire.get(slot, (0, 0, 0))[('hits', 'kills', 'damage').index(metric)]
                            if scenario == 'firing_lane' else 0)
                writer.writerow(row)


class EvaluationTests(unittest.TestCase):
    def test_v3_unchanged_other_shooter_kill_does_not_block_worker(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            attributed_team_reports(root, {'primary': (2, 1, 10)},
                                    {'primary': (2, 1, 10)})
            report = summarize_team(root, 'worker', (152239,), 480,
                                    'revision', protocol_version=3)
            self.assertTrue(report['eligible'])
            self.assertEqual('paired-team-damage-friendly-v3',
                             report['protocol']['gate'])
            self.assertEqual(1, report['summary']['candidate']['friendly_kills'])
            self.assertEqual(0, report['summary']['candidate']['role_friendly_kills'])

    def test_v3_candidate_shooter_kill_or_damage_blocks_worker(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            attributed_team_reports(root, {'primary': (2, 1, 10)},
                                    {'ally': (2, 1, 10)})
            report = summarize_team(root, 'worker', (152239,), 480,
                                    'revision', protocol_version=3)
            self.assertFalse(report['eligible'])
            self.assertEqual({'role_friendly_kills', 'role_friendly_damage'},
                             {item['metric'] for item in report['regressions']})
            attributed_team_reports(root, {'primary': (2, 0, 10)},
                                    {'primary': (2, 0, 7), 'ally': (1, 0, 3)})
            report = summarize_team(root, 'worker', (152239,), 480,
                                    'revision', protocol_version=3)
            self.assertEqual({'role_friendly_damage'},
                             {item['metric'] for item in report['regressions']})

    def test_v3_global_worsening_and_bad_attribution_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            attributed_team_reports(root, {'primary': (1, 0, 4)},
                                    {'primary': (2, 1, 8)})
            report = summarize_team(root, 'worker', (152239,), 480,
                                    'revision', protocol_version=3)
            self.assertEqual({'friendly_damage', 'friendly_kills'},
                             {item['metric'] for item in report['regressions']})
            path = root / '152239-team-candidate.csv'
            with path.open() as source:
                rows = list(csv.DictReader(source))
            rows[0]['ally_friendly_hits'] = '1'
            with path.open('w') as out:
                writer = csv.DictWriter(out, fieldnames=rows[0])
                writer.writeheader(); writer.writerows(rows)
            with self.assertRaisesRegex(ValueError, 'attribution total mismatch'):
                summarize_team(root, 'worker', (152239,), 480,
                               'revision', protocol_version=3)

    def test_investigation_rejects_hidden_tracking_and_inert_candidate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            fields = ['policy', 'scenario', 'archetype', 'seed',
                      'brain_generation', 'elapsed_frames',
                      'first_observer_sight_frame', 'first_ally_report_frame',
                      'observer_sight_frames', 'ally_sight_frames',
                      'observer_investigate_frames', 'ally_investigate_frames',
                      'observer_recover_frames', 'ally_recover_frames',
                      'observer_travel', 'ally_travel', 'ally_source_at_report',
                      'ally_source_final', 'max_ally_knowledge_age_seconds',
                      'ally_report_error', 'ally_hidden_player_error',
                      'report_refreshes_without_reacquisition',
                      'unseen_target_updates', 'hidden_visual_contacts',
                      'neural_ticks']
            def write(policy, **overrides):
                row = dict(policy='neural', scenario='lost_sight', archetype='guard',
                           seed=152201, brain_generation=200 if policy == 'active' else 910,
                           elapsed_frames=480, first_observer_sight_frame=1,
                           first_ally_report_frame=1, observer_sight_frames=30,
                           ally_sight_frames=0, observer_investigate_frames=200,
                           ally_investigate_frames=60, observer_recover_frames=140,
                           ally_recover_frames=150, observer_travel=6,
                           ally_travel=4, ally_source_at_report='ally_report',
                           ally_source_final='none', max_ally_knowledge_age_seconds=4,
                           ally_report_error=0.4, ally_hidden_player_error=4,
                           report_refreshes_without_reacquisition=0,
                           unseen_target_updates=0, hidden_visual_contacts=0,
                           neural_ticks=30)
                row.update(overrides)
                with (root / f'152201-investigation-{policy}.csv').open('w') as out:
                    writer = csv.DictWriter(out, fieldnames=fields)
                    writer.writeheader(); writer.writerow(row)
            write('active'); write('candidate')
            report = summarize_investigation(root, 'guard', (152201,), 480, 'revision')
            self.assertTrue(report['eligible'])
            write('candidate', report_refreshes_without_reacquisition=1)
            self.assertFalse(summarize_investigation(root, 'guard', (152201,),
                                                      480, 'revision')['eligible'])
            write('candidate', observer_travel=0.1)
            self.assertFalse(summarize_investigation(root, 'guard', (152201,),
                                                      480, 'revision')['eligible'])
            write('candidate', observer_investigate_frames=0,
                  ally_investigate_frames=0)
            self.assertFalse(summarize_investigation(root, 'guard', (152201,),
                                                      480, 'revision')['eligible'])
            write('candidate', observer_sight_frames=28)
            report = summarize_investigation(root, 'guard', (152201,), 480, 'revision')
            self.assertFalse(report['eligible'])
            self.assertEqual('investigation_contract', report['regressions'][0]['metric'])
            self.assertEqual({'observer_sight_frames': 28},
                             report['regressions'][0]['values'])
            write('active', observer_sight_frames=28)
            with self.assertRaisesRegex(ValueError, 'incomplete investigation'):
                summarize_investigation(root, 'guard', (152201,), 480, 'revision')

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
