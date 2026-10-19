"""Synthetic tests for exact-file CycloneDX Windows SBOM generator."""
from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from generate_windows_sbom import digest_file, generate_bom

SHA = 'a' * 40


class WindowsSbomTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.base = Path(tmp.name)
        self.root = self.base / 'RcloneBrowser'
        fixtures = {
            'RcloneBrowser.exe': b'synthetic app binary',
            'rclone/rclone.exe': b'synthetic rclone binary',
            'Qt6Core.dll': b'synthetic Qt DLL',
            'msvcp140.dll': b'synthetic Microsoft DLL',
            'icuuc.dll': b'synthetic ICU DLL',
            'RcloneBrowser.ini': b'[Settings]\nrclone=rclone/rclone.exe\n',
            'qt.conf': b'[Paths]\nPlugins = qt/plugins\n',
            'docs/BUILD_PROVENANCE.txt': (
                f'source_commit={SHA}\nqt_version=6.10.3\n'
                'rclone_version=v1.75.2\n'
            ).encode(),
            'docs/LICENSE': b'MIT synthetic notice',
            'icons/remotes/cloud.png': b'synthetic icon',
        }
        for relative, content in fixtures.items():
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
        self.refresh_manifest()
        self.inventory_path = self.base / 'binary-inventory.json'
        self.rows = []
        for path, family in [
            ('RcloneBrowser.exe', 'project_application'),
            ('rclone/rclone.exe', 'upstream_rclone'),
            ('Qt6Core.dll', 'qt_framework'),
            ('msvcp140.dll', 'microsoft_vc_runtime'),
            ('icuuc.dll', 'icu_runner_catalog_signature'),
        ]:
            f = self.root / path
            self.rows.append({
                'path': path, 'family': family,
                'sha256': digest_file(f),
                'size_bytes': f.stat().st_size,
                'pe_file_version': 'synthetic',
                'signature_status': 'Valid', 'signature_type': 'synthetic',
                'signer_subject': 'synthetic test signer',
            })
        self.refresh_inventory()

    def refresh_manifest(self):
        lines = []
        for p in sorted(self.root.rglob('*')):
            if p.is_file() and p.name != 'PACKAGE_SHA256SUMS.txt':
                lines.append(f'{digest_file(p)}  {p.relative_to(self.root).as_posix()}')
        target = self.root / 'docs/PACKAGE_SHA256SUMS.txt'
        target.write_text('\n'.join(lines) + '\n', encoding='ascii')

    def refresh_inventory(self):
        self.inventory_path.write_text(json.dumps({
            'schema': 'rclonebrowser.windows-dependencies.v1',
            'versions': {'application': '4.0.2', 'qt': '6.10.3',
                         'rclone': '1.75.2'},
            'file_count': len(self.rows), 'files': self.rows,
        }), encoding='utf-8')

    def generate(self):
        return generate_bom(self.root, self.inventory_path, commit=SHA,
                            app='4.0.2', qt='6.10.3', rclone='v1.75.2')

    def test_exact_files_and_sha_and_versions(self):
        result = self.generate()
        files = [c for c in result['components'] if c['type'] == 'file']
        self.assertEqual(result['bomFormat'], 'CycloneDX')
        self.assertEqual(result['specVersion'], '1.6')
        self.assertEqual(len(files), 11)
        self.assertEqual(len(result['components']), 16)
        exe = next(c for c in files if c['name'] == 'RcloneBrowser.exe')
        self.assertEqual(exe['hashes'][0]['content'],
                         digest_file(self.root / 'RcloneBrowser.exe'))
        self.assertTrue(result['serialNumber'].startswith('urn:uuid:'))

    def test_unexpected_file_without_manifest_entry_fails(self):
        (self.root / 'secrets.ini').write_text('synthetic')
        with self.assertRaisesRegex(ValueError, 'exact package payload'):
            self.generate()

    def test_tampered_file_fails(self):
        (self.root / 'docs/LICENSE').write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError, 'differs from manifest'):
            self.generate()

    def test_missing_manifest_fails(self):
        (self.root / 'docs/PACKAGE_SHA256SUMS.txt').unlink()
        with self.assertRaisesRegex(ValueError, 'Missing packaged SHA'):
            self.generate()

    def test_repeated_manifest_entry_fails(self):
        file = self.root / 'docs/PACKAGE_SHA256SUMS.txt'
        lines = file.read_text().splitlines()
        file.write_text('\n'.join(lines + [lines[0]]) + '\n')
        with self.assertRaisesRegex(ValueError, 'repeated'):
            self.generate()

    def test_wrong_provenance_fails(self):
        p = self.root / 'docs/BUILD_PROVENANCE.txt'
        p.write_text(p.read_text().replace(SHA, 'b'*40))
        self.refresh_manifest()
        with self.assertRaisesRegex(ValueError, 'Unexpected provenance'):
            self.generate()

    def test_incomplete_binary_inventory_fails(self):
        self.rows.pop()
        self.refresh_inventory()
        with self.assertRaisesRegex(ValueError, 'all packaged EXE/DLL'):
            self.generate()

    def test_stale_binary_inventory_fails(self):
        self.rows[0]['sha256'] = '0'*64
        self.refresh_inventory()
        with self.assertRaisesRegex(ValueError, 'Outdated or altered'):
            self.generate()

    def test_unreviewed_binary_family_fails(self):
        self.rows[0]['family'] = 'unreviewed'
        self.refresh_inventory()
        with self.assertRaisesRegex(ValueError, 'Unreviewed dependency family'):
            self.generate()

    def test_unknown_license_not_incorrectly_claimed(self):
        b = self.generate()
        logical = next(c for c in b['components'] if c['bom-ref'] == 'logical:Qt')
        self.assertNotIn('licenses', logical)
        upstream = next(c for c in b['components'] if c['bom-ref'] == 'logical:rclone')
        self.assertEqual(upstream['licenses'][0]['license']['id'], 'MIT')
        unknown_file = next(c for c in b['components']
                            if c.get('name') == 'icons/remotes/cloud.png')
        self.assertNotIn('licenses', unknown_file)

    def test_incorrect_build_version_fails(self):
        with self.assertRaisesRegex(ValueError, 'versions do not match build'):
            generate_bom(self.root, self.inventory_path, commit=SHA,
                         app='4.0.3', qt='6.10.3', rclone='v1.75.2')


if __name__ == '__main__':
    unittest.main()
