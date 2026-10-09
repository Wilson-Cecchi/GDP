import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch

import updater


def manifest(version="0.1.3", url=None):
    tag = f"public-v{version}"
    return json.dumps({
        "schema_version": 1,
        "version": version,
        "tag": tag,
        "source_sha": "a" * 40,
        "assets": {"linux-x64": {
            "file": updater.ARCHIVE_NAME,
            "size": 7,
            "sha256": "b" * 64,
            "url": url or f"https://github.com/{updater.REPOSITORY}/releases/download/{tag}/{updater.ARCHIVE_NAME}",
        }},
    }).encode()


class LinuxUpdaterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.keys = tempfile.TemporaryDirectory()
        key_dir = Path(cls.keys.name)
        cls.signing_key = key_dir / "signing.pem"
        cls.public_key = key_dir / "public.pem"
        cls.other_public_key = key_dir / "other-public.pem"
        subprocess.run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt",
                        "rsa_keygen_bits:2048", "-out", str(cls.signing_key)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["openssl", "pkey", "-in", str(cls.signing_key), "-pubout",
                        "-out", str(cls.public_key)], check=True)
        other_key = key_dir / "other.pem"
        subprocess.run(["openssl", "genpkey", "-algorithm", "RSA", "-pkeyopt",
                        "rsa_keygen_bits:2048", "-out", str(other_key)],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        subprocess.run(["openssl", "pkey", "-in", str(other_key), "-pubout",
                        "-out", str(cls.other_public_key)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.keys.cleanup()

    def sign_manifest(self, raw):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "update.json"
            signature_path = Path(temporary) / "update.json.sig"
            path.write_bytes(raw)
            subprocess.run(["openssl", "dgst", "-sha256", "-sign", str(self.signing_key),
                            "-sigopt", "rsa_padding_mode:pss", "-sigopt",
                            "rsa_pss_saltlen:-1", "-out", str(signature_path), str(path)],
                           check=True)
            return signature_path.read_bytes()

    def test_signed_manifest_is_required_before_parsing(self):
        raw = manifest()
        signature = self.sign_manifest(raw)
        with patch.object(updater, "PUBLIC_KEY", self.public_key), \
             patch.object(updater, "fetch_limited", side_effect=[raw, signature]) as fetch:
            self.assertEqual(updater.fetch_manifest("0.1.2")["version"], "0.1.3")
            self.assertEqual([call.args[0] for call in fetch.call_args_list],
                             [updater.MANIFEST_URL, updater.SIGNATURE_URL])
        with patch.object(updater, "PUBLIC_KEY", self.public_key), \
             patch.object(updater, "fetch_limited", side_effect=[raw + b" ", signature]), \
             patch.object(updater, "parse_manifest") as parse:
            with self.assertRaisesRegex(updater.UpdateError, "signature is invalid"):
                updater.fetch_manifest("0.1.2")
            parse.assert_not_called()
        with patch.object(updater, "PUBLIC_KEY", self.other_public_key), \
             patch.object(updater, "fetch_limited", side_effect=[raw, signature]), \
             patch.object(updater, "parse_manifest") as parse:
            with self.assertRaisesRegex(updater.UpdateError, "signature is invalid"):
                updater.fetch_manifest("0.1.2")
            parse.assert_not_called()
        with patch.object(updater, "fetch_limited",
                          side_effect=[raw, updater.UpdateError("missing signature")]), \
             patch.object(updater, "parse_manifest") as parse:
            with self.assertRaisesRegex(updater.UpdateError, "missing signature"):
                updater.fetch_manifest("0.1.2")
            parse.assert_not_called()

    def test_newer_release_is_selected(self):
        result = updater.parse_manifest(manifest(), "0.1.2")
        self.assertEqual(result["version"], "0.1.3")
        self.assertIsNone(updater.parse_manifest(manifest(), "0.1.3"))
        self.assertIsNone(updater.parse_manifest(manifest(), "0.2.0"))

    def test_rejects_foreign_url_and_malformed_manifest(self):
        with self.assertRaises(updater.UpdateError):
            updater.parse_manifest(manifest(url="https://example.com/game.tar.gz"), "0.1.2")
        with self.assertRaises(updater.UpdateError):
            updater.parse_manifest(b"[]", "0.1.2")
        with self.assertRaises(updater.UpdateError):
            updater.parse_manifest(b"not json", "0.1.2")

    def test_extraction_rejects_path_traversal_and_links(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            archive_path = root / "bad.tar.gz"
            for member in ("../outside", f"{updater.PACKAGE_NAME}/evil"):
                with tarfile.open(archive_path, "w:gz") as archive:
                    entry = tarfile.TarInfo(member)
                    entry.size = 1
                    if member.endswith("evil"):
                        entry.type = tarfile.SYMTYPE
                        entry.linkname = "../outside"
                    archive.addfile(entry, io.BytesIO(b"x") if entry.isfile() else None)
                with self.assertRaises(updater.UpdateError):
                    updater.extract_archive(archive_path, root / "unpacked")
            self.assertFalse((root / "outside").exists())

    def test_marker_changes_only_after_verified_install(self):
        with tempfile.TemporaryDirectory() as root:
            root = Path(root)
            marker = root / "current-version.txt"
            marker.write_text("0.1.2\n")

            def fake_extract(_archive, destination):
                package = destination / updater.PACKAGE_NAME
                package.mkdir(parents=True)
                return package

            asset = {"version": "0.1.3", "url": "ignored", "size": 7, "sha256": "b" * 64}
            with patch.object(updater, "download_archive"), \
                 patch.object(updater, "extract_archive", side_effect=fake_extract), \
                 patch.object(updater, "verify_package", side_effect=updater.UpdateError("bad")):
                with self.assertRaises(updater.UpdateError):
                    updater.install_update(root, asset)
            self.assertEqual(marker.read_text(), "0.1.2\n")
            with patch.object(updater, "download_archive"), \
                 patch.object(updater, "extract_archive", side_effect=fake_extract), \
                 patch.object(updater, "verify_package"):
                updater.install_update(root, asset)
            self.assertEqual(marker.read_text(), "0.1.3\n")
            self.assertEqual((root / "previous-version.txt").read_text(), "0.1.2\n")
            self.assertTrue((root / "versions" / "0.1.3" / updater.PACKAGE_NAME).is_dir())

    def test_real_archive_install_and_rollback(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            archive_path = root / "candidate.tar.gz"
            contents = {"version.txt": b"0.1.3\n", "gdp": b"#!/bin/sh\nexit 0\n",
                        "updater.py": b"# updater\n",
                        "release-signing-public.pem": self.public_key.read_bytes(),
                        "bin/gdp": b"#!/bin/sh\nif [ \"$1\" = --version ]; then echo 'Iron Seed 0.1.3'; fi\n"}
            contents.update({f"bin/ai/{brain}.brain": b"brain" for brain in updater.BRAINS})
            with tarfile.open(archive_path, "w:gz") as archive:
                for name, data in contents.items():
                    entry = tarfile.TarInfo(f"{updater.PACKAGE_NAME}/{name}")
                    entry.size = len(data)
                    archive.addfile(entry, io.BytesIO(data))

            old = {name: data.replace(b"0.1.3", b"0.1.2") for name, data in contents.items()
                   if name != "release-signing-public.pem"}
            for name, data in old.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                if name in ("gdp", "bin/gdp"):
                    path.chmod(0o755)

            asset = {"version": "0.1.3", "url": "ignored", "size": archive_path.stat().st_size,
                     "sha256": "ignored"}
            with patch.object(updater, "download_archive",
                              side_effect=lambda _asset, dest: shutil.copyfile(archive_path, dest)):
                updater.install_update(root, asset)
            self.assertEqual((root / "current-version.txt").read_text(), "0.1.3\n")
            self.assertEqual((root / "previous-version.txt").read_text(), "base\n")
            updater.rollback_update(root)
            self.assertEqual((root / "current-version.txt").read_text(), "base\n")
            self.assertTrue((root / "versions" / "0.1.3" / updater.PACKAGE_NAME).exists())


if __name__ == "__main__":
    unittest.main()
