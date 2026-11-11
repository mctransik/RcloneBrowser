#!/usr/bin/env python3
"""Validate Windows PE inventory against signed-publisher policy and package bytes.

No PE file is executed. Signature authenticity is supplied by Windows'
Get-AuthenticodeSignature step; this script validates its exported metadata.
"""
from __future__ import annotations
import argparse
import csv
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys

VC_RUNTIME = {
    'concrt140.dll', 'msvcp140.dll', 'msvcp140_1.dll', 'msvcp140_2.dll',
    'msvcp140_atomic_wait.dll', 'msvcp140_codecvt_ids.dll',
    'vccorlib140.dll', 'vcruntime140.dll', 'vcruntime140_1.dll',
    'vcruntime140_threads.dll',
}
QT_PLUGINS = {'iconengines', 'imageformats', 'platforms', 'tls'}
HEX = re.compile(r'[0-9a-fA-F]{64}\Z')


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def _classify(row: dict[str, str], path: str, app: str, qt: str, rclone: str) -> str:
    status, kind, signer = (row[k] for k in
                            ('signature_status', 'signature_type', 'signer_subject'))
    version = row['file_version_metadata']
    if path in ('RcloneBrowser.exe', 'rclone/rclone.exe'):
        want = app if path == 'RcloneBrowser.exe' else rclone
        if status != 'NotSigned' or signer or kind != 'None' or version != want:
            raise ValueError(f'Unexpected app/rclone signature or version: {path}')
        return 'project_application' if path == 'RcloneBrowser.exe' else 'upstream_rclone'
    qt_path = bool(re.fullmatch(r'Qt6[A-Za-z0-9]+\.dll', path)) or bool(
        re.fullmatch(r'qt/plugins/([a-z]+)/[A-Za-z0-9_]+\.dll', path) and
        path.split('/')[2] in QT_PLUGINS)
    if qt_path:
        if (status != 'Valid' or kind != 'Authenticode' or
            'CN=The QT Company Oy,' not in signer or
            'O=The QT Company Oy,' not in signer or
            not version.startswith(qt + '.')):
            raise ValueError(f'Unexpected Qt signing identity or version: {path}')
        return 'qt_framework'
    if path in VC_RUNTIME:
        if (status != 'Valid' or kind != 'Authenticode' or
            'CN=Microsoft Windows Software Compatibility Publisher,' not in signer or
            'O=Microsoft Corporation,' not in signer or
            not version.startswith('14.')):
            raise ValueError(f'Unexpected Microsoft runtime signature: {path}')
        return 'microsoft_vc_runtime'
    if path == 'icuuc.dll':
        # A catalog signature on the GitHub runner is NOT an embedded signature.
        if (status != 'Valid' or kind != 'Catalog' or
            'CN=Microsoft Windows,' not in signer or
            'O=Microsoft Corporation,' not in signer):
            raise ValueError('Unexpected ICU catalog signature; manual review needed')
        return 'icu_runner_catalog_signature'
    raise ValueError(f'Unapproved Windows executable dependency: {path}')


def audit_dependencies(package_dir: Path, csv_path: Path, *, app: str,
                       qt: str, rclone: str) -> dict:
    if not package_dir.is_dir() or package_dir.is_symlink():
        raise ValueError('Windows package directory unavailable or linked')
    with csv_path.open('r', encoding='utf-8-sig', newline='') as stream:
        reader = csv.DictReader(stream)
        expected_headers = {
            'relative_path', 'sha256', 'file_size_bytes', 'signature_status',
            'signature_type', 'signer_subject', 'file_version_metadata',
        }
        if not reader.fieldnames or not expected_headers.issubset(reader.fieldnames):
            raise ValueError('Signature CSV missing required columns')
        rows = list(reader)
    if not rows:
        raise ValueError('Empty signature inventory')
    files = {}
    for p in package_dir.rglob('*'):
        if p.is_file() and p.suffix.lower() in ('.exe', '.dll'):
            rel = p.relative_to(package_dir).as_posix()
            if p.is_symlink() or (hasattr(p, 'is_junction') and p.is_junction()):
                raise ValueError(f'Linked dependency not allowed: {rel}')
            files[rel] = p
    if len(rows) != len(files):
        raise ValueError('Signature report does not cover every EXE/DLL')
    found = set()
    inventory = []
    for row in rows:
        rel = row['relative_path']
        pure = PurePosixPath(rel)
        if (pure.is_absolute() or '..' in pure.parts or '.' in pure.parts or
            '\\' in rel or rel not in files or rel in found):
            raise ValueError(f'Invalid, duplicate or unexpected report path: {rel}')
        found.add(rel)
        p = files[rel]
        digest = row['sha256']
        if (not HEX.fullmatch(digest) or digest.lower() != _sha256(p) or
            str(p.stat().st_size) != row['file_size_bytes']):
            raise ValueError(f'Signature report file hash/size mismatch: {rel}')
        family = _classify(row, rel, app, qt, rclone)
        inventory.append({
            'path': rel, 'family': family, 'size_bytes': p.stat().st_size,
            'sha256': digest.lower(), 'pe_file_version': row['file_version_metadata'],
            'signature_status': row['signature_status'],
            'signature_type': row['signature_type'],
            'signer_subject': row['signer_subject'],
        })
    families = [item['family'] for item in inventory]
    for family in ('project_application', 'upstream_rclone', 'qt_framework',
                   'microsoft_vc_runtime', 'icu_runner_catalog_signature'):
        if family not in families:
            raise ValueError(f'Required dependency family missing: {family}')
    return {
        'schema': 'rclonebrowser.windows-dependencies.v1',
        'assessment': 'inventory_and_runner_signature_policy_only_not_malware_clearance',
        'versions': {'application': app, 'qt': qt, 'rclone': rclone},
        'file_count': len(inventory),
        'files': sorted(inventory, key=lambda x: x['path'].casefold()),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package-dir', type=Path, required=True)
    parser.add_argument('--authenticode-csv', type=Path, required=True)
    parser.add_argument('--app-version', required=True)
    parser.add_argument('--qt-version', required=True)
    parser.add_argument('--rclone-version', required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        data = audit_dependencies(args.package_dir, args.authenticode_csv,
                                  app=args.app_version, qt=args.qt_version,
                                  rclone=args.rclone_version.lstrip('v'))
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(data, indent=2, ensure_ascii=True) + '\n',
                               encoding='utf-8')
    except (OSError, ValueError, UnicodeError) as exc:
        print(f'Windows component identity check FAILED: {exc}', file=sys.stderr)
        return 1
    print(f"Windows component identity check passed: {data['file_count']} "
          'EXE/DLL files with expected family, signer and version metadata.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
