#!/usr/bin/env python3
"""Run untouched final seeds explicitly; never use them to rank candidates."""
import argparse
import fcntl
from pathlib import Path
import signal
import tempfile
import train_service as service
from training_evaluation import (summarize, summarize_team,
                                 summarize_investigation, combine_new_mechanics,
                                 combine_perception)

FINAL_SEEDS = (10007, 11003, 13001, 17011, 19001, 23003, 29009, 31013)
NEW_MECHANICS_FINAL_SEEDS = (151001, 151013, 151027, 151039,
                             151051, 151061, 151073, 151099)


def final_plan(cooperative, protocol):
    if cooperative and protocol in (3, 4):
        return FINAL_SEEDS, ('maps', 'duel', 'team', 'investigation')
    if cooperative and protocol == 2:
        return NEW_MECHANICS_FINAL_SEEDS, ('maps', 'duel', 'team')
    return FINAL_SEEDS, ('maps', 'duel')


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
        protocol = service.TRAIN_PROTOCOL if service.COOPERATIVE else 1
        seeds, modes = final_plan(service.COOPERATIVE, protocol)
        for seed in seeds:
            for mode in modes:
                for candidate in (False, True):
                    if service.STOP:
                        raise SystemExit(143)
                    policy = 'candidate' if candidate else 'active'
                    command = [str(service.APP/'gdp_ai_gameplay_eval'), '--brain-dir',
                        str(service.APP/'assets/ai'), '--seed', str(seed), '--frames', '1800']
                    if mode == 'investigation':
                        command += ['--investigation', '--investigation-role', args.role]
                    elif mode == 'team' and protocol == 4:
                        command.append('--team-attributed')
                    elif mode != 'maps':
                        command.append(f'--{mode}')
                    if candidate: command += ['--brain', f'{args.role}={args.brain}']
                    if service.run(command,destination/f'{seed}-{mode}-{policy}.csv',
                                   args.role,'final_evaluation',80,70) != 0:
                        raise SystemExit(1)
        solo = summarize(destination,args.role,seeds,1800,revision)
        if protocol in (3, 4):
            team = summarize_team(destination,args.role,seeds,1800,revision,
                                  protocol_version=3 if protocol == 4 else 2)
            investigation = summarize_investigation(destination,args.role,
                                                    seeds,1800,revision)
            report = combine_perception(solo, team, investigation,
                                        team_protocol=3 if protocol == 4 else 2)
        elif protocol == 2:
            report = combine_new_mechanics(solo,
                summarize_team(destination,args.role,seeds,1800,revision,
                               protocol_version=2))
        else:
            report = solo
        service.atomic_json(destination/'report.json', report)
        service.atomic_json(service.DATA/'status.json', {'state':'stopped'})
        service.event('final_evaluation_complete',report=str(destination/'report.json'))


if __name__ == '__main__':
    signal.signal(signal.SIGTERM,service.request_stop)
    signal.signal(signal.SIGINT,service.request_stop)
    main()
