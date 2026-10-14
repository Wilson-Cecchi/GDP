#!/usr/bin/env python3
"""Run untouched final seeds explicitly; never use them to rank candidates."""
import argparse
import fcntl
from pathlib import Path
import signal
import tempfile
import train_service as service
from training_evaluation import summarize, summarize_team, combine_new_mechanics

FINAL_SEEDS = (10007, 11003, 13001, 17011, 19001, 23003, 29009, 31013)
NEW_MECHANICS_FINAL_SEEDS = (151001, 151013, 151027, 151039,
                             151051, 151061, 151073, 151099)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--role', choices=service.ROLES, required=True)
    parser.add_argument('--brain', type=Path, required=True)
    args = parser.parse_args()
    if not service.valid_checkpoint(args.brain, args.role):
        parser.error('invalid candidate brain')
    service.DATA.mkdir(parents=True, exist_ok=True)
    with (service.DATA/'service.lock').open('a') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        root = service.DATA/'final-evaluations'/args.role
        root.mkdir(parents=True, exist_ok=True)
        destination = Path(tempfile.mkdtemp(prefix='final-', dir=root))
        revision = (service.APP/'SOURCE_COMMIT').read_text().strip()
        new_mechanics = service.COOPERATIVE and service.TRAIN_PROTOCOL == 2
        seeds = NEW_MECHANICS_FINAL_SEEDS if new_mechanics else FINAL_SEEDS
        for seed in seeds:
            for mode in (('maps', 'duel', 'team') if new_mechanics
                         else ('maps', 'duel')):
                for candidate in (False, True):
                    if service.STOP:
                        raise SystemExit(143)
                    policy = 'candidate' if candidate else 'active'
                    command = [str(service.APP/'gdp_ai_gameplay_eval'), '--brain-dir',
                        str(service.APP/'assets/ai'), '--seed', str(seed), '--frames', '1800']
                    if mode != 'maps': command.append(f'--{mode}')
                    if candidate: command += ['--brain', f'{args.role}={args.brain}']
                    if service.run(command,destination/f'{seed}-{mode}-{policy}.csv',
                                   args.role,'final_evaluation',80,70) != 0:
                        raise SystemExit(1)
        solo = summarize(destination,args.role,seeds,1800,revision)
        report = (combine_new_mechanics(solo,
            summarize_team(destination,args.role,seeds,1800,revision,
                           protocol_version=2)) if new_mechanics else solo)
        service.atomic_json(destination/'report.json', report)
        service.atomic_json(service.DATA/'status.json', {'state':'stopped'})
        service.event('final_evaluation_complete',report=str(destination/'report.json'))


if __name__ == '__main__':
    signal.signal(signal.SIGTERM,service.request_stop)
    signal.signal(signal.SIGINT,service.request_stop)
    main()
