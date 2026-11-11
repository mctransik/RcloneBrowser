"""Synthetic-only checks of Windows dependency identity and integrity policy."""
from __future__ import annotations
import csv
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from audit_windows_dependencies import audit_dependencies, _sha256

QT = 'CN=The QT Company Oy, O=The QT Company Oy, C=FI'
MS = 'CN=Microsoft Windows Software Compatibility Publisher, O=Microsoft Corporation, C=US'
ICU = 'CN=Microsoft Windows, O=Microsoft Corporation, C=US'
HEADERS = ['relative_path', 'file_size_bytes', 'sha256', 'signature_status',
           'signature_type', 'signer_subject', 'file_version_metadata']


class WindowsDependencyTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.base = Path(tmp.name)
        self.package = self.base / 'RcloneBrowser'
        self.package.mkdir()
        self.report = self.base / 'auth.csv'
        sources = [
            ('RcloneBrowser.exe', 'NotSigned', 'None', '', '4.0.2'),
            ('rclone/rclone.exe', 'NotSigned', 'None', '', '1.75.2'),
            ('Qt6Core.dll', 'Valid', 'Authenticode', QT, '6.10.3.0'),
            ('qt/plugins/platforms/qwindows.dll', 'Valid', 'Authenticode', QT, '6.10.3.0'),
            ('msvcp140.dll', 'Valid', 'Authenticode', MS, '14.51.36247.0'),
            ('icuuc.dll', 'Valid', 'Catalog', ICU, '72, 1, 0, 4'),
        ]
        self.rows = []
        for i, (name, status, kind, subject, ver) in enumerate(sources):
            p = self.package / name
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(f'synthetic {name} {i}'.encode())
            self.rows.append({'relative_path': name, 'file_size_bytes': str(p.stat().st_size),
                              'sha256': _sha256(p), 'signature_status': status,
                              'signature_type': kind, 'signer_subject': subject,
                              'file_version_metadata': ver})
        self.refresh_report()

    def refresh_report(self):
        with self.report.open('w', encoding='utf-8', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=HEADERS)
            writer.writeheader()
            writer.writerows(self.rows)

    def audit(self):
        return audit_dependencies(self.package, self.report,
                                  app='4.0.2', qt='6.10.3', rclone='1.75.2')

    def test_valid_expected_dependencies(self):
        result = self.audit()
        self.assertEqual(result['file_count'], 6)
        self.assertEqual(len(result['files']), 6)
        self.assertEqual(result['versions']['qt'], '6.10.3')

    def test_changed_dll_rejected(self):
        (self.package / 'Qt6Core.dll').write_bytes(b'MZ changed')
        with self.assertRaisesRegex(ValueError, 'hash/size mismatch'):
            self.audit()

    def test_unknown_dll_rejected_even_when_signed(self):
        src = self.package / 'Qt6Core.dll'
        other = self.package / 'unreviewed.dll'
        other.write_bytes(src.read_bytes())
        row = dict(self.rows[2], relative_path='unreviewed.dll')
        self.rows.append(row)
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'Unapproved'):
            self.audit()

    def test_unsigned_qt_rejected(self):
        self.rows[2]['signature_status'] = 'NotSigned'
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'Qt signing'):
            self.audit()

    def test_wrong_qt_signer_rejected(self):
        self.rows[2]['signer_subject'] = MS
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'Qt signing'):
            self.audit()

    def test_wrong_qt_version_rejected(self):
        self.rows[2]['file_version_metadata'] = '6.10.2.0'
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'Qt signing'):
            self.audit()

    def test_old_embedded_app_version_rejected(self):
        self.rows[0]['file_version_metadata'] = '4.0.1'
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'app/rclone'):
            self.audit()

    def test_missing_signature_row_rejected(self):
        self.rows.pop()
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'does not cover'):
            self.audit()

    def test_duplicate_signature_row_rejected(self):
        self.rows.append(dict(self.rows[0]))
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'does not cover'):
            self.audit()

    def test_path_traversal_rejected(self):
        self.rows[0]['relative_path'] = '../RcloneBrowser.exe'
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'report path'):
            self.audit()

    def test_catalog_icu_signature_required(self):
        self.rows[-1]['signature_type'] = 'Authenticode'
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'ICU catalog'):
            self.audit()

    def test_fake_microsoft_signature_rejected(self):
        self.rows[4]['signer_subject'] = QT
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'Microsoft runtime'):
            self.audit()

    def test_unsanctioned_rclone_version_rejected(self):
        self.rows[1]['file_version_metadata'] = '1.75.1'
        self.refresh_report()
        with self.assertRaisesRegex(ValueError, 'app/rclone'):
            self.audit()

    def test_exe_missing_on_disk_rejected(self):
        (self.package / 'RcloneBrowser.exe').unlink()
        with self.assertRaisesRegex(ValueError, 'does not cover'):
            self.audit()


if __name__ == '__main__':
    unittest.main()
