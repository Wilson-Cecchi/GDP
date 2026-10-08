import tempfile
import unittest
from pathlib import Path

from make_release_manifest import ASSETS, make_manifest


class ReleaseManifestTests(unittest.TestCase):
    def test_describes_all_packages(self):
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root)
            for name in ASSETS.values():
                (directory / name).write_bytes(b"package")
            result = make_manifest("public-v1.2.3", "a" * 40, directory)
            self.assertEqual(result["version"], "1.2.3")
            self.assertEqual(set(result["assets"]), set(ASSETS))
            self.assertEqual(result["assets"]["linux-x64"]["size"], 7)
            self.assertIn("/public-v1.2.3/", result["assets"]["windows-x64"]["url"])

    def test_rejects_missing_or_empty_package(self):
        with tempfile.TemporaryDirectory() as root:
            directory = Path(root)
            with self.assertRaises(ValueError):
                make_manifest("public-v1.2.3", "a" * 40, directory)
            for name in ASSETS.values():
                (directory / name).write_bytes(b"package")
            (directory / ASSETS["windows-x64"]).write_bytes(b"")
            with self.assertRaises(ValueError):
                make_manifest("public-v1.2.3", "a" * 40, directory)

    def test_rejects_invalid_identity(self):
        with tempfile.TemporaryDirectory() as root:
            with self.assertRaises(ValueError):
                make_manifest("public-v1.2", "a" * 40, Path(root))
            with self.assertRaises(ValueError):
                make_manifest("public-v1.2.3", "wrong", Path(root))


if __name__ == "__main__":
    unittest.main()
