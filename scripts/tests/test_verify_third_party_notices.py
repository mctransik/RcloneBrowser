"""Synthetic fail-closed checks for bundled third-party license notices."""
from __future__ import annotations

from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify_third_party_notices import LICENSE_FILES, verify_notices

REPO = Path(__file__).resolve().parents[2]


class ThirdPartyNoticeTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.source = Path(tmp.name) / "source"
        self.notices = Path(tmp.name) / "packaged"
        self.notices.mkdir()
        for name, relative in LICENSE_FILES.items():
            target = self.source / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            original = REPO / relative
            target.write_bytes(original.read_bytes())
            (self.notices / name).write_bytes(original.read_bytes())

    def verify(self, version="v1.75.2"):
        return verify_notices(self.source, self.notices, version)

    def test_original_texts_and_notices_pass(self):
        self.assertEqual(self.verify(), 3)

    def test_changed_rclone_version_fails(self):
        with self.assertRaisesRegex(ValueError, "Unreviewed rclone version"):
            self.verify("v1.75.3")

    def test_deleted_rclone_license_fails(self):
        (self.notices / "RCLONE_LICENSE.txt").unlink()
        with self.assertRaisesRegex(ValueError, "unavailable"):
            self.verify()

    def test_changed_packaged_fluent_license_fails(self):
        (self.notices / "FLUENT_UI_SYSTEM_ICONS_LICENSE.txt").write_text("wrong")
        with self.assertRaisesRegex(ValueError, "changed in package"):
            self.verify()

    def test_changed_vendored_rclone_source_and_package_fails(self):
        source = self.source / LICENSE_FILES["RCLONE_LICENSE.txt"]
        content = source.read_bytes() + b"\n"
        source.write_bytes(content)
        (self.notices / "RCLONE_LICENSE.txt").write_bytes(content)
        with self.assertRaisesRegex(ValueError, "pinned upstream Git blob"):
            self.verify()

    def test_changed_fluent_source_and_package_fails(self):
        source = self.source / LICENSE_FILES["FLUENT_UI_SYSTEM_ICONS_LICENSE.txt"]
        content = source.read_bytes() + b"\n"
        source.write_bytes(content)
        (self.notices / "FLUENT_UI_SYSTEM_ICONS_LICENSE.txt").write_bytes(content)
        with self.assertRaisesRegex(ValueError, "Fluent license text changed"):
            self.verify()

    def test_missing_license_warning_fails_even_when_packaged_matches_source(self):
        source = self.source / LICENSE_FILES["THIRD_PARTY_NOTICES.md"]
        changed = source.read_text().replace("LICENSE REVIEW INCOMPLETE", "review")
        source.write_text(changed)
        (self.notices / "THIRD_PARTY_NOTICES.md").write_text(changed)
        with self.assertRaisesRegex(ValueError, "missing a required"):
            self.verify()

    def test_notice_symlink_rejected(self):
        path = self.notices / "RCLONE_LICENSE.txt"
        target = path.with_suffix(".external")
        shutil.copyfile(path, target)
        path.unlink()
        try:
            path.symlink_to(target)
        except (OSError, NotImplementedError):
            self.skipTest("Symlinks unsupported on runner")
        with self.assertRaisesRegex(ValueError, "unavailable or linked"):
            self.verify()

    def test_missing_source_license_fails(self):
        (self.source / LICENSE_FILES["FLUENT_UI_SYSTEM_ICONS_LICENSE.txt"]).unlink()
        with self.assertRaisesRegex(ValueError, "unavailable"):
            self.verify()


if __name__ == "__main__":
    unittest.main()
