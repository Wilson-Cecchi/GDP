"""The final gate must exercise the same modes as protocol-four selection."""

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools/zimaos'))
spec = importlib.util.spec_from_file_location(
    'evaluate_final', Path(__file__).resolve().parents[1]
    / 'tools/zimaos/evaluate_final.py')
evaluate_final = importlib.util.module_from_spec(spec)
spec.loader.exec_module(evaluate_final)


class FinalEvaluationTests(unittest.TestCase):
    def test_protocol_four_runs_attributed_team_and_investigation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'SOURCE_COMMIT').write_text('test-revision\n')
            brain = root / 'guard.brain'
            brain.write_text('candidate')
            service = evaluate_final.service
            with patch.object(service, 'COOPERATIVE', True), \
                 patch.object(service, 'TRAIN_PROTOCOL', 4), \
                 patch.object(service, 'DATA', root / 'data'), \
                 patch.object(service, 'APP', root), \
                 patch.object(service, 'STOP', False), \
                 patch.object(service, 'valid_checkpoint', return_value=True), \
                 patch.object(service, 'run', return_value=0) as run, \
                 patch.object(service, 'event'), \
                 patch.object(evaluate_final, 'summarize', return_value={}), \
                 patch.object(evaluate_final, 'summarize_team', return_value={}) as team, \
                 patch.object(evaluate_final, 'summarize_investigation',
                              return_value={}) as investigation, \
                 patch.object(evaluate_final, 'combine_perception',
                              return_value={'eligible': True}) as combine, \
                 patch.object(sys, 'argv', ['evaluate_final.py', '--role',
                                           'guard', '--brain', str(brain)]):
                evaluate_final.main()
            commands = [call.args[0] for call in run.call_args_list]
            self.assertEqual(8 * 4 * 2, len(commands))
            self.assertEqual(16, sum('--team-attributed' in command
                                     for command in commands))
            self.assertEqual(16, sum('--investigation' in command
                                     for command in commands))
            self.assertTrue(all('--team' not in command for command in commands))
            self.assertEqual(3, team.call_args.kwargs['protocol_version'])
            self.assertEqual(8, len(investigation.call_args.args[2]))
            self.assertEqual(3, combine.call_args.kwargs['team_protocol'])


if __name__ == '__main__':
    unittest.main()
