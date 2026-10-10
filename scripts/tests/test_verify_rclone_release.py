"""Offline tests with a dedicated public key and a pre-signed fixture.

These tests deliberately do NOT generate keys: the Windows Git GnuPG
distribution may support signature verification but not local key creation.
The private fixture key is never distributed or needed.
"""
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify_rclone_release import verify_release

TEST_SIGNER = "A2C8667C62D81AD2BCC1FDE72DC39979F424D5AA"
FIXTURES = Path(__file__).resolve().parent / "fixtures"


class SignatureTests(unittest.TestCase):
    def setUp(self):
        self.gpg = os.getenv("GPG_BINARY", "gpg")
        self.temp = tempfile.TemporaryDirectory(prefix="pgp-fixture-")
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.zip = root / "rclone-v1.2.3-windows-amd64.zip"
        self.zip.write_bytes(b"Safe unit test archive")
        self.sums = root / "SHA256SUMS"
        self.sums.write_bytes((FIXTURES / "SHA256SUMS.asc").read_bytes())
        self.key = FIXTURES / "public.asc"

    def verify(self, permitted=frozenset({TEST_SIGNER})):
        return verify_release(self.zip, self.sums, self.key, self.gpg, permitted)

    def test_correct_signature_and_digest(self):
        signer, digest = self.verify()
        self.assertEqual(signer, TEST_SIGNER)
        self.assertEqual(digest, "c39ee68504638d2efc8b4cc121be33427985a334515bfdbbfea4f8653a8cb9ca")

    def test_changed_archive_rejected(self):
        self.zip.write_bytes(b"Altered archive content")
        with self.assertRaisesRegex(ValueError, "checksum"):
            self.verify()

    def test_changed_signed_text_rejected(self):
        self.sums.write_bytes(self.sums.read_bytes().replace(b"rclone-v1.2.3", b"rclone-v9.9.9"))
        with self.assertRaisesRegex(ValueError, "PGP signature"):
            self.verify()

    def test_wrong_signing_identity_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unexpected signer"):
            self.verify(frozenset({"0" * 40}))

    def test_missing_named_archive_rejected(self):
        self.zip.rename(self.zip.with_name("different-archive.zip"))
        self.zip = self.zip.with_name("different-archive.zip")
        with self.assertRaisesRegex(ValueError, "exactly one"):
            self.verify()


if __name__ == "__main__":
    unittest.main()
