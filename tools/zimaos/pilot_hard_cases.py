#!/usr/bin/env python3
"""Run a bounded, isolated curriculum-19/hard-case A/B pilot; never promote brains."""

import argparse
import hashlib
import os
from pathlib import Path
import signal
import sys

sys.path.insert(0, os.environ.get('GDP_APP_DIR', '/opt/gdp'))
import train_service as service
from training_evaluation import summarize, summarize_team, combine_new_mechanics


DEVELOPMENT_SEEDS = (150001, 150011)
TRAINING_SEEDS = (20261014, 20261015)
ROLES = ('guard', 'robot')
FRAMES = 480


def digest(path):
    with path.open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def run_evaluator(destination, role, seed, mode, candidate=None):
    policy = 'candidate' if candidate else 'active'
    csv_path = destination / f'{seed}-{mode}-{policy}.csv'
    command = [str(service.APP / 'gdp_ai_gameplay_eval'), '--brain-dir',
               str(service.APP / 'assets/ai'), '--seed', str(seed),
               '--frames', str(FRAMES)]
    if mode != 'maps':
        command.append(f'--{mode}')
    if candidate:
        command += ['--brain', f'{role}={candidate}']
    if service.run(command, csv_path, role, 'pilot_evaluation', 80, 70) != 0:
        raise RuntimeError(f'pilot evaluation failed: {role} {seed} {mode} {policy}')
    return csv_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generations', type=int, default=28)
    parser.add_argument('--warmup-generations', type=int, default=7)
    parser.add_argument('--variant', choices=('v1', 'v2'), default='v1')
    args = parser.parse_args()
    if (not 1 <= args.warmup_generations <= args.generations <= 70
            or not service.COOPERATIVE or service.TRAIN_PROTOCOL != 2):
        parser.error('requires cooperative protocol 2 and 1..70 generations')
    revision = (service.APP / 'SOURCE_COMMIT').read_text().strip()
    binaries = {name: digest(service.APP / name)
                for name in ('gdp', 'gdp_ai_gameplay_eval')}
    active_brains = {role: digest(service.APP / 'assets/ai' / f'{role}.brain')
                     for role in service.ROLES}
    root = service.DATA / f'hard-case-pilot-{args.variant}'
    if root.exists():
        parser.error(f'pilot output already exists: {root}')
    root.mkdir(parents=True)
    plan = {'source_revision': revision, 'binary_sha256': binaries,
            'active_brain_sha256': active_brains,
            'pilot_script_sha256': digest(Path(__file__)),
            'variant': args.variant,
            'training_seeds': TRAINING_SEEDS,
            'development_seeds': DEVELOPMENT_SEEDS,
            'frames': FRAMES, 'generations': args.generations,
            'warmup_generations': args.warmup_generations,
            'population': 8, 'roles': ROLES, 'results': []}
    service.atomic_json(root / 'manifest.json', plan)
    for role in ROLES:
        baseline = root / role / 'active'
        baseline.mkdir(parents=True)
        for seed in DEVELOPMENT_SEEDS:
            for mode in ('maps', 'duel', 'team'):
                run_evaluator(baseline, role, seed, mode)
        for training_seed in TRAINING_SEEDS:
            # Reverse the arm order for the second seed to reduce ordering bias.
            arms = ('standard', 'hard_case') if training_seed == TRAINING_SEEDS[0] \
                else ('hard_case', 'standard')
            for arm in arms:
                if service.STOP:
                    raise SystemExit(143)
                path = root / role / str(training_seed) / arm
                path.mkdir(parents=True)
                runs = path / 'runs'
                command = [str(service.APP / 'gdp'), '--ai-training', role,
                           '--cooperative-brain', '--all-scenarios',
                           '--diverse-maps', '--population', '8',
                           '--generations', str(args.generations),
                           '--curriculum-generations',
                           str(args.warmup_generations), '--seed',
                           str(training_seed), '--cooldown-ms', '1000',
                           '--save-dir', str(runs)]
                if arm == 'hard_case':
                    command.append('--hard-case-curriculum-v2'
                                   if args.variant == 'v2'
                                   else '--hard-case-curriculum')
                service.event('pilot_arm_start', role=role, seed=training_seed,
                              arm=arm)
                if service.run(command, path / 'training.log', role,
                               'pilot_training', 80, 70) != 0:
                    raise RuntimeError(f'pilot training failed: {role} {training_seed} {arm}')
                champion = service.latest_checkpoint(runs, role)
                if champion is None:
                    raise RuntimeError('pilot training produced no brain')
                evaluation = path / 'evaluation'
                evaluation.mkdir()
                for seed in DEVELOPMENT_SEEDS:
                    for mode in ('maps', 'duel', 'team'):
                        active = baseline / f'{seed}-{mode}-active.csv'
                        (evaluation / active.name).write_bytes(active.read_bytes())
                        run_evaluator(evaluation, role, seed, mode, champion)
                solo = summarize(evaluation, role, DEVELOPMENT_SEEDS, FRAMES,
                                 revision)
                team = summarize_team(evaluation, role, DEVELOPMENT_SEEDS,
                                      FRAMES, revision, protocol_version=2)
                report = combine_new_mechanics(solo, team)
                service.atomic_json(evaluation / 'report.json', report)
                row = {'role': role, 'training_seed': training_seed, 'arm': arm,
                       'generation': report['generation'],
                       'brain': str(champion), 'brain_sha256': digest(champion),
                       'score': report['score'],
                       'regressions': len(report['regressions']),
                       'eligible': report['eligible'],
                       'report': str(evaluation / 'report.json')}
                plan['results'].append(row)
                service.atomic_json(root / 'manifest.json', plan)
                service.event('pilot_arm_complete', **row)
    plan['complete'] = True
    service.atomic_json(root / 'manifest.json', plan)


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, service.request_stop)
    signal.signal(signal.SIGINT, service.request_stop)
    main()
