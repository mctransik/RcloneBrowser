"""No-network verification of ZIP hash handoff to attestation."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify_windows_scanned_artifact import verify_scanned_archive


class ScannedArchiveHandoffTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.archive = self.root / "RcloneBrowser-4.0.2-windows-x64.zip"
        self.archive.write_bytes(b"synthetic ZIP data not executable")
        self.checksum = self.root / "windows-zip.sha256"
        self.sha = hashlib.sha256(self.archive.read_bytes()).hexdigest()
        self.checksum.write_text(
            f"{self.sha}  staging/{self.archive.name}\n", encoding="utf-8"
        )

    def test_valid_hash_and_filename(self):
        self.assertEqual(verify_scanned_archive(self.checksum, self.archive), self.sha)

    def test_changed_file_rejected(self):
        self.archive.write_bytes(b"tampered file")
        with self.assertRaisesRegex(ValueError, "digest differs"):
            verify_scanned_archive(self.checksum, self.archive)

    def test_different_name_rejected(self):
        other = self.root / "RcloneBrowser-4.0.3-windows-x64.zip"
        other.write_bytes(self.archive.read_bytes())
        with self.assertRaisesRegex(ValueError, "filename differs"):
            verify_scanned_archive(self.checksum, other)

    def test_multiple_records_rejected(self):
        self.checksum.write_text(self.checksum.read_text() * 2)
        with self.assertRaisesRegex(ValueError, "exactly one"):
            verify_scanned_archive(self.checksum, self.archive)

    def test_unexpected_path_rejected(self):
        self.checksum.write_text(f"{self.sha}  ../../../evil.zip\n")
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            verify_scanned_archive(self.checksum, self.archive)

    def test_malformed_digest_rejected(self):
        self.checksum.write_text(f"badsha  staging/{self.archive.name}\n")
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            verify_scanned_archive(self.checksum, self.archive)


if __name__ == "__main__":
    unittest.main()
