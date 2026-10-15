#!/usr/bin/env python3
"""Capture paired gameplay CSVs for a frozen five-brain cooperative bundle."""

import argparse
import hashlib
import os
from pathlib import Path
import signal
import sys

sys.path.insert(0, os.environ.get('GDP_APP_DIR', '/opt/gdp'))
import train_service as service


STAGES = {
    'quick': ((150001, 150011), 480),
    'wide': ((150001, 150011, 150031, 150053), 1200),
}


def digest(path):
    with Path(path).open('rb') as source:
        return hashlib.file_digest(source, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--brain', action='append', required=True,
                        metavar='ROLE=FILE')
    parser.add_argument('--label', required=True)
    parser.add_argument('--stage', choices=STAGES, default='wide')
    args = parser.parse_args()
    if not service.COOPERATIVE or service.TRAIN_PROTOCOL != 2:
        parser.error('requires cooperative protocol 2')
    if not args.label.isascii() or not args.label.replace('-', '').isalnum():
        parser.error('label must contain only ASCII letters, digits and hyphens')
    candidates = {}
    for raw in args.brain:
        role, separator, filename = raw.partition('=')
        if not separator or role not in service.ROLES or role in candidates or not filename:
            parser.error(f'invalid or duplicate brain: {raw}')
        candidates[role] = Path(filename)
    if set(candidates) != set(service.ROLES):
        parser.error('exactly one candidate brain per role is required')
    for role, path in candidates.items():
        if not service.valid_checkpoint(path, role):
            parser.error(f'invalid cooperative brain: {role}={path}')
    seeds, frames = STAGES[args.stage]
    root = service.DATA / 'bundle-evaluations' / args.label / args.stage
    if root.exists():
        parser.error(f'bundle evaluation already exists: {root}')
    root.mkdir(parents=True)
    revision = (service.APP / 'SOURCE_COMMIT').read_text().strip()
    manifest = {
        'evaluator_source_revision': revision,
        'evaluator_sha256': digest(service.APP / 'gdp_ai_gameplay_eval'),
        'active_brain_sha256': {
            role: digest(service.APP / 'assets/ai' / f'{role}.brain')
            for role in service.ROLES},
        'candidate_brains': {
            role: {'path': str(candidates[role]), 'sha256': digest(candidates[role])}
            for role in service.ROLES},
        'script_sha256': digest(__file__),
        'label': args.label,
        'stage': args.stage,
        'seeds': seeds,
        'frames': frames,
        'modes': ('maps', 'duel', 'team'),
        'completed': [],
    }
    service.atomic_json(root / 'manifest.json', manifest)
    for seed in seeds:
        for mode in manifest['modes']:
            for policy in ('active', 'bundle'):
                if service.STOP:
                    raise SystemExit(143)
                path = root / f'{seed}-{mode}-{policy}.csv'
                command = [str(service.APP / 'gdp_ai_gameplay_eval'),
                           '--brain-dir', str(service.APP / 'assets/ai'),
                           '--seed', str(seed), '--frames', str(frames)]
                if mode != 'maps':
                    command.append(f'--{mode}')
                if policy == 'bundle':
                    for role in service.ROLES:
                        command += ['--brain', f'{role}={candidates[role]}']
                if service.run(command, path, 'bundle', 'bundle_evaluation', 80, 70) != 0:
                    raise RuntimeError(f'bundle evaluation failed: {seed} {mode} {policy}')
                manifest['completed'].append(path.name)
                service.atomic_json(root / 'manifest.json', manifest)
    manifest['complete'] = True
    service.atomic_json(root / 'manifest.json', manifest)
    service.event('bundle_evaluation_complete', label=args.label,
                  stage=args.stage, root=str(root))


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, service.request_stop)
    signal.signal(signal.SIGINT, service.request_stop)
    main()
