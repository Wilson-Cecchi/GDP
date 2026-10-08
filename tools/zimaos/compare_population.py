#!/usr/bin/env python3
"""Paired, bounded population 8/16 pilot from the same frozen champion weights."""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import signal
import tempfile
import time
import train_service as service


def seed_copy(source, target, role, seed):
    lines = source.read_text().splitlines()
    for index, key, value in ((3,'seed',seed),(4,'generation',0),(5,'id',0),(6,'fitness_bits',0)):
        lines[index] = f'{key} {value}'
    body = '\n'.join(lines[:-1])+'\n'
    checksum = 14695981039346656037
    for byte in body.encode():
        checksum = ((checksum ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    target.write_text(body+f'checksum {checksum}\n')
    if not service.valid_checkpoint(target,role):
        raise ValueError('failed to create validated pilot seed brain')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--role',choices=service.ROLES,default='guard')
    parser.add_argument('--brain',type=Path,required=True)
    parser.add_argument('--generations',type=int,default=7)
    args=parser.parse_args()
    if not 1 <= args.generations <= 10000 or not service.valid_checkpoint(args.brain,args.role):
        parser.error('invalid generation count or starting checkpoint')
    with (service.DATA/'service.lock').open('a') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        root=service.DATA/'experiments';root.mkdir(parents=True,exist_ok=True)
        experiment=Path(tempfile.mkdtemp(prefix='population-8-16-',dir=root))
        revision=(service.APP/'SOURCE_COMMIT').read_text().strip()
        manifest={'role':args.role,'generations':args.generations,'source_revision':revision,
                  'starting_brain':str(args.brain),
                  'starting_sha256':hashlib.sha256(args.brain.read_bytes()).hexdigest(),
                  'training_seeds':[60013,60017],'arms':[]}
        service.atomic_json(experiment/'comparison.json',manifest)
        for seed in manifest['training_seeds']:
            input_dir=experiment/f'seed-{seed}';input_dir.mkdir()
            starting=input_dir/f'{args.role}_generation_000000.brain'
            seed_copy(args.brain,starting,args.role,seed)
            for population in (8,16):
                if service.STOP: raise SystemExit(143)
                arm=input_dir/f'population-{population}';arm.mkdir()
                runs=arm/'runs'
                command=[str(service.APP/'gdp'),'--ai-training',args.role,'--all-scenarios',
                         '--diverse-maps','--population',str(population),'--generations',
                         str(args.generations),'--cooldown-ms','1000','--resume',str(starting),
                         '--save-dir',str(runs)]
                started=time.monotonic()
                if service.run(command,arm/'training.log',args.role,'population_comparison',80,70)!=0:
                    raise SystemExit(1)
                elapsed=time.monotonic()-started
                champion=service.latest_checkpoint(runs,args.role)
                report=service.evaluate_candidate(args.role,champion,arm/'evaluation',
                    (7,1001,2027,4099),1200,revision,80,70,'population-pilot')
                if report is None: raise SystemExit(143)
                manifest['arms'].append({'seed':seed,'population':population,
                    'training_seconds':round(elapsed,2),'report':str(arm/'evaluation/report.json'),
                    'eligible':report['eligible'],'score':report['score'],'summary':report['summary'],
                    'regressions':report['regressions']})
                service.atomic_json(experiment/'comparison.json',manifest)
        manifest['completed']=True
        service.atomic_json(experiment/'comparison.json',manifest)
        service.atomic_json(service.DATA/'status.json',{'state':'stopped'})
        service.event('population_comparison_complete',report=str(experiment/'comparison.json'))


if __name__ == '__main__':
    signal.signal(signal.SIGTERM,service.request_stop)
    signal.signal(signal.SIGINT,service.request_stop)
    main()
