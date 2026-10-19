"""Synthetic-only regression tests for Windows release payload guard."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from audit_release_payload import audit_package, hash_file


class ReleasePayloadGuardTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / "RcloneBrowser"
        samples = {
            "RcloneBrowser.exe": b"MZ synthetic stub",
            "RcloneBrowser.ini": b"[Settings]\nrclone=rclone/rclone.exe\ncheckRcloneUpdates=true\n",
            "qt.conf": b"[Paths]\nPlugins = qt/plugins\n",
            "Qt6Core.dll": b"synthetic DLL",
            "rclone/rclone.exe": b"MZ synthetic rclone",
            "qt/plugins/platforms/qwindows.dll": b"synthetic plugin",
            "icons/remotes/cloud.png": b"synthetic PNG",
            "icons/remotes/README.md": b"remote icons",
            "docs/README.md": b"project readme",
            "docs/LICENSE": b"test license",
            "docs/CHANGELOG.md": b"test changelog",
            "docs/Rclone-Browser-Guide.pdf": b"%PDF-1.4 synthetic",
            "docs/BUILD_PROVENANCE.txt": b"source_commit=000\n",
            "docs/RCLONE_LICENSE.txt": b"synthetic rclone license",
            "docs/FLUENT_UI_SYSTEM_ICONS_LICENSE.txt": b"synthetic Fluent license",
            "docs/THIRD_PARTY_NOTICES.md": b"synthetic incomplete-license notice",
        }
        for name, content in samples.items():
            p = self.root / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(content)
        self.refresh_manifest()

    def refresh_manifest(self):
        lines = []
        for p in sorted(self.root.rglob("*")):
            if p.is_file() and p.name != "PACKAGE_SHA256SUMS.txt":
                lines.append(f"{hash_file(p)}  {p.relative_to(self.root).as_posix()}")
        (self.root / "docs/PACKAGE_SHA256SUMS.txt").write_text(
            "\n".join(lines) + "\n", encoding="ascii")

    def test_expected_package_passes(self):
        self.assertEqual(audit_package(self.root), 17)

    def test_missing_notice_is_blocked_even_with_updated_manifest(self):
        (self.root / "docs/RCLONE_LICENSE.txt").unlink()
        self.refresh_manifest()
        with self.assertRaisesRegex(ValueError, "Missing required"):
            audit_package(self.root)

    def test_extra_unreviewed_license_file_is_blocked(self):
        (self.root / "docs/UNREVIEWED_LICENSE.txt").write_text("unknown")
        self.refresh_manifest()
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            audit_package(self.root)

    def test_rclone_conf_is_blocked_even_with_valid_manifest(self):
        (self.root / "rclone.conf").write_text("[cloud]\ntoken=secret")
        self.refresh_manifest()
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            audit_package(self.root)

    def test_unexpected_program_is_blocked(self):
        (self.root / "updater.exe").write_bytes(b"MZ")
        self.refresh_manifest()
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            audit_package(self.root)

    def test_private_key_file_is_blocked(self):
        (self.root / "docs/id_ed25519").write_text("private key")
        self.refresh_manifest()
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            audit_package(self.root)

    def test_modified_portable_config_is_blocked(self):
        (self.root / "RcloneBrowser.ini").write_text(
            "[Settings]\nrclone=rclone/rclone.exe\ncheckRcloneUpdates=true\n"
            "token=secret\n")
        self.refresh_manifest()
        with self.assertRaisesRegex(ValueError, "portable settings"):
            audit_package(self.root)

    def test_secret_marker_in_readme_is_blocked(self):
        (self.root / "docs/README.md").write_text(
            "-----BEGIN OPENSSH PRIVATE KEY-----\n")
        self.refresh_manifest()
        with self.assertRaisesRegex(ValueError, "Potential private credential"):
            audit_package(self.root)

    def test_missing_required_file_is_blocked(self):
        (self.root / "rclone/rclone.exe").unlink()
        self.refresh_manifest()
        with self.assertRaisesRegex(ValueError, "Missing required"):
            audit_package(self.root)

    def test_changed_member_without_manifest_update_is_blocked(self):
        (self.root / "Qt6Core.dll").write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "manifest"):
            audit_package(self.root)

    def test_missing_hash_line_is_blocked(self):
        m = self.root / "docs/PACKAGE_SHA256SUMS.txt"
        m.write_text("",
                     encoding="ascii")
        with self.assertRaisesRegex(ValueError, "manifest"):
            audit_package(self.root)

    def test_hidden_directory_is_blocked(self):
        (self.root / ".ssh").mkdir()
        with self.assertRaisesRegex(ValueError, "Unexpected"):
            audit_package(self.root)

    def test_symlink_file_is_blocked(self):
        target = self.root / "Qt6Core.dll"
        path = self.root / "Qt6Linked.dll"
        try:
            path.symlink_to(target)
        except (OSError, NotImplementedError):
            self.skipTest("File symlinks not permitted in this environment")
        with self.assertRaisesRegex(ValueError, "linked"):
            audit_package(self.root)


if __name__ == "__main__":
    unittest.main()
