"""Offline tests of Windows hash reports using fake disposable program bytes."""
import hashlib
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from audit_historical_windows_artifacts import collect_windows_pe_hashes, write_tsv

class HistoricalBinaryHashTests(unittest.TestCase):
    def test_hash_exe_and_dll_but_not_documentation(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "rclone").mkdir()
            binaries = {
                "RcloneBrowser.exe": b"synthetic exe only",
                "rclone/rclone.exe": b"synthetic rclone only",
                "Qt6Core.DLL": b"synthetic DLL only",
            }
            for name, blob in binaries.items():
                (root / name).write_bytes(blob)
            (root / "README.md").write_text("not a binary", encoding="utf-8")
            found = collect_windows_pe_hashes(root)
            by_path = {path: (size, digest) for path, size, digest in found}
            self.assertEqual(set(binaries), set(by_path))
            for name, blob in binaries.items():
                self.assertEqual(by_path[name], (len(blob), hashlib.sha256(blob).hexdigest()))
            output = root / "manifest.tsv"
            write_tsv(output, ("name", "sha256"), [(p, digest) for p, _, digest in found])
            self.assertIn("RcloneBrowser.exe", output.read_text(encoding="utf-8"))
            self.assertNotIn("README.md", output.read_text(encoding="utf-8"))

    def test_missing_rclone_executable_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            (root / "RcloneBrowser.exe").write_bytes(b"fake")
            (root / "Qt6Core.dll").write_bytes(b"fake")
            with self.assertRaisesRegex(RuntimeError, "rclone.exe"):
                collect_windows_pe_hashes(root)

if __name__ == "__main__":
    unittest.main()
