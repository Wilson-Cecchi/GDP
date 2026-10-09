"""Check unattended resume and thermal pause behavior without loading the CPU."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import signal
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/zimaos"))

spec = importlib.util.spec_from_file_location("train_service",
    Path(__file__).resolve().parents[1] / "tools/zimaos/train_service.py")
service = importlib.util.module_from_spec(spec)
spec.loader.exec_module(service)


def write_brain(path, generation, role="worker", weight="0", identity=0,
                cooperative=False):
    count = 305 if cooperative else 221
    body = (f"GDP_ENEMY_BRAIN {4 if cooperative else 3}\n"
            f"archetype {service.ROLES.index(role)}\n"
            f"contract {3 if cooperative else 2}\nseed 42\n"
            f"generation {generation}\nid {identity}\n"
            f"fitness_bits 0\nweights {count}\n" + (weight + "\n") * count)
    checksum = 14695981039346656037
    for byte in body.encode():
        checksum = ((checksum ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    path.write_text(body + f"checksum {checksum}\n")


class TrainingServiceTests(unittest.TestCase):
    def test_cooperative_checkpoint_cannot_resume_tactical_population(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            brain = root / 'guard_generation_000002.brain'
            write_brain(brain, 2, role='guard', cooperative=True)
            self.assertFalse(service.valid_checkpoint(brain, 'guard'))
            with patch.object(service, 'COOPERATIVE', True):
                self.assertTrue(service.valid_checkpoint(brain, 'guard'))
                write_brain(brain, 2, role='guard')
                self.assertFalse(service.valid_checkpoint(brain, 'guard'))

    def test_cooperative_population_v2_can_resume(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            population = root / 'guard_generation_000002.population'
            population.mkdir()
            body = ('GDP_TRAINING_POPULATION 2\narchetype 1\nseed 42\n'
                    'generation 2\npopulation 2\nreward_rules 6\ncontract 3\n'
                    'curriculum 15\nwarmup_start 1\nwarmup_generations 2\n')
            checksum = 14695981039346656037
            for byte in body.encode():
                checksum = ((checksum ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
            (population / 'state.txt').write_text(body + f'checksum {checksum}\n')
            for identity in range(2):
                write_brain(population / f'member-{identity:03d}.brain', 2,
                            role='guard', identity=identity, cooperative=True)
            with patch.object(service, 'COOPERATIVE', True):
                self.assertEqual(population,
                    service.latest_population(root, 'guard', 2))
            self.assertIsNone(service.population_metadata(population, 'guard', 2))

    def test_resume_uses_newest_valid_checkpoint(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for generation in (1, 2):
                write_brain(root / f"worker_generation_{generation:06d}.brain", generation)
            bad = root / "worker_generation_000003.brain"
            bad.write_text("interrupted record")
            self.assertEqual("worker_generation_000002.brain",
                service.latest_checkpoint(root, "worker").name)

    def test_corrupt_only_directory_cannot_silently_restart_fresh(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "worker_generation_000001.brain").write_text("bad record")
            with self.assertRaises(RuntimeError):
                service.latest_checkpoint(root, "worker")

    def test_record_checks_archetype_and_finite_weights(self):
        with tempfile.TemporaryDirectory() as directory:
            brain = Path(directory) / "worker_generation_000001.brain"
            write_brain(brain, 1)
            self.assertTrue(service.valid_checkpoint(brain, "worker"))
            self.assertFalse(service.valid_checkpoint(brain, "guard"))
            write_brain(brain, 1, weight=str(0x7FC00000))
            self.assertFalse(service.valid_checkpoint(brain, "worker"))
            write_brain(brain, 1, weight=str(0x40000000))
            self.assertFalse(service.valid_checkpoint(brain, "worker"))

    def test_cpu_sensor_required_and_uses_hottest_core(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(service, "HWMON", root):
                with self.assertRaises(RuntimeError):
                    service.temperature()
                cpu = root / "hwmon8"
                cpu.mkdir()
                (cpu / "name").write_text("coretemp\n")
                (cpu / "temp1_input").write_text("41000\n")
                (cpu / "temp2_input").write_text("44000\n")
                self.assertEqual(44.0, service.temperature())

    def test_thermal_guard_pauses_then_resumes_same_process(self):
        class Child:
            pid = 123
            returncode = 0
            signals = []
            checks = 0
            def poll(self):
                self.checks += 1
                return None if self.checks <= 2 else 0
            def send_signal(self, value):
                self.signals.append(value)
        child = Child()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with patch.object(service, "DATA", root), \
                 patch.object(service, "STOP", False), \
                 patch.object(service, "temperature", side_effect=[40, 81, 69]), \
                 patch.object(service.subprocess, "Popen", return_value=child), \
                 patch.object(service.time, "sleep"):
                self.assertEqual(0, service.run(["trainer"], root / "run.log",
                                               "worker", "training", 80, 70))
            self.assertEqual([signal.SIGSTOP, signal.SIGCONT], child.signals)

    def test_full_population_requires_all_members_and_unique_ids(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            population = root / "worker_generation_000002.population"
            population.mkdir()
            body = ("GDP_TRAINING_POPULATION 1\narchetype 0\nseed 42\ngeneration 2\n"
                    "population 2\nreward_rules 5\ncurriculum 15\nwarmup_start 1\n"
                    "warmup_generations 7\n")
            checksum = 14695981039346656037
            for byte in body.encode():
                checksum = ((checksum ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
            (population / "state.txt").write_text(body + f"checksum {checksum}\n")
            write_brain(population / "member-000.brain", 2, identity=0)
            write_brain(population / "member-001.brain", 2, identity=1)
            self.assertEqual(population, service.latest_population(root, "worker", 2))
            self.assertIsNone(service.population_metadata(population, "guard", 2))
            self.assertIsNone(service.population_metadata(population, "worker", 8))
            write_brain(population / "member-001.brain", 2, identity=0)
            self.assertIsNone(service.population_metadata(population, "worker", 2))
            (population / "member-001.brain").unlink()
            with self.assertRaises(RuntimeError):
                service.latest_population(root, "worker", 2)

    def test_missing_sensor_does_not_launch_training(self):
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(service, "temperature", side_effect=RuntimeError("no sensor")), \
                 patch.object(service.subprocess, "Popen") as launch:
                with self.assertRaises(RuntimeError):
                    service.run(["trainer"], Path(directory) / "run.log",
                                "worker", "training", 80, 70)
                launch.assert_not_called()


if __name__ == "__main__":
    unittest.main()
