#!/usr/bin/env python3
"""Evaluate one isolated cooperative candidate on a development set."""

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
    parser.add_argument('--role', choices=service.ROLES, required=True)
    parser.add_argument('--brain', type=Path, required=True)
    parser.add_argument('--label', required=True)
    parser.add_argument('--stage', choices=STAGES, default='wide')
    args = parser.parse_args()
    if not service.COOPERATIVE or service.TRAIN_PROTOCOL != 2:
        parser.error('requires cooperative protocol 2')
    if not service.valid_checkpoint(args.brain, args.role):
        parser.error('invalid cooperative candidate brain')
    if not args.label.isascii() or not args.label.replace('-', '').isalnum():
        parser.error('label must contain only ASCII letters, digits and hyphens')
    seeds, frames = STAGES[args.stage]
    root = service.DATA / f'isolated-{args.stage}' / args.label / args.role
    if root.exists():
        parser.error(f'evaluation already exists: {root}')
    root.mkdir(parents=True)
    revision = (service.APP / 'SOURCE_COMMIT').read_text().strip()
    provenance = {
        'source_revision': revision,
        'binary_sha256': {
            name: digest(service.APP / name)
            for name in ('gdp', 'gdp_ai_gameplay_eval')},
        'active_brain_sha256': {
            role: digest(service.APP / 'assets/ai' / f'{role}.brain')
            for role in service.ROLES},
        'candidate': str(args.brain),
        'candidate_sha256': digest(args.brain),
        'stage': args.stage,
        'seeds': seeds,
        'frames': frames,
        'label': args.label,
        'evaluation_script_sha256': digest(__file__),
    }
    service.atomic_json(root / 'manifest.json', provenance)
    report = service.evaluate_candidate(args.role, args.brain,
        root / 'evaluation', seeds, frames, revision, 80, 70,
        f'{args.label}-{args.stage}')
    if report is None:
        raise SystemExit(143)
    provenance['complete'] = True
    provenance['eligible'] = report['eligible']
    provenance['score'] = report['score']
    provenance['regressions'] = len(report['regressions'])
    service.atomic_json(root / 'manifest.json', provenance)


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, service.request_stop)
    signal.signal(signal.SIGINT, service.request_stop)
    main()
