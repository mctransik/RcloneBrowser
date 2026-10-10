"""Local signed-data test cases; do not contact release servers."""
import hashlib
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify_rclone_release import verify_release


class SignatureTests(unittest.TestCase):
    def setUp(self):
        self.gpg = os.getenv("GPG_BINARY", "gpg")
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        base = Path(self.temp.name)
        self.home = base / "gpg"
        self.home.mkdir()
        self.zip = base / "rclone-v1.2.3-windows-amd64.zip"
        self.zip.write_bytes(b"Safe unit test archive")
        self.sums = base / "SHA256SUMS"
        self.key = base / "test-public.asc"
        self.args = [self.gpg, "--batch", "--no-tty", "--homedir", str(self.home),
                     "--pinentry-mode", "loopback"]
        self.run_gpg(["--passphrase", "", "--quick-generate-key",
                      "Test signing key <test@example.invalid>", "ed25519", "sign", "0"])
        listing = self.run_gpg(["--with-colons", "--list-keys"]).stdout.decode()
        self.signer = next(line.split(":")[9] for line in listing.splitlines()
                           if line.startswith("fpr:"))
        self.key.write_bytes(self.run_gpg(["--armor", "--export", self.signer]).stdout)
        self.sign(hashlib.sha256(self.zip.read_bytes()).hexdigest() + "  " + self.zip.name + "\n")

    def run_gpg(self, args):
        return subprocess.run(self.args + args, check=True, capture_output=True, timeout=40)

    def sign(self, text):
        path = self.sums.parent / "plaintext.txt"
        path.write_text(text, encoding="utf-8")
        self.run_gpg(["--yes", "--local-user", self.signer, "--armor",
                      "--clearsign", "--output", str(self.sums), str(path)])

    def check(self, allowed=None):
        if allowed is None:
            allowed = frozenset({self.signer})
        return verify_release(self.zip, self.sums, self.key, self.gpg, allowed)

    def test_correct_signature_and_digest(self):
        self.assertEqual(self.check()[0], self.signer)

    def test_changed_archive_rejected(self):
        self.zip.write_bytes(b"Altered")
        with self.assertRaisesRegex(ValueError, "checksum"):
            self.check()

    def test_changed_signed_text_rejected(self):
        self.sums.write_bytes(self.sums.read_bytes().replace(b"rclone-v1.2.3", b"rclone-v9.9.9"))
        with self.assertRaisesRegex(ValueError, "PGP signature"):
            self.check()

    def test_wrong_signing_identity_rejected(self):
        with self.assertRaisesRegex(ValueError, "Unexpected signer"):
            self.check(frozenset({"0" * 40}))

    def test_missing_named_archive_rejected(self):
        self.sign("a" * 64 + "  different.zip\n")
        with self.assertRaisesRegex(ValueError, "exactly one"):
            self.check()


if __name__ == "__main__":
    unittest.main()
