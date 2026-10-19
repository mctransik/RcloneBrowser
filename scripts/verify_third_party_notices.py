#!/usr/bin/env python3
"""Fail closed when pinned rclone/Fluent notices are absent or altered.

Checks exact upstream rclone v1.75.2 COPYING Git blob, in-repo Fluent
MIT license, and the package's explicit incomplete-license notice.
Run before hashing/packing the Windows payload and Linux AppDir.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys

PINNED_RCLONE_VERSION = "v1.75.2"
UPSTREAM_RCLONE_GIT_BLOB = "8c27c67fd0a12a1e18db983f19450abed435e689"
FLUENT_GIT_BLOB = "bc9c36b28f467f7006c4e5f1be4ca0a1a0b895f3"

LICENSE_FILES = {
    "RCLONE_LICENSE.txt": "third_party/licenses/rclone-v1.75.2-COPYING.txt",
    "FLUENT_UI_SYSTEM_ICONS_LICENSE.txt": "src/icons/fluent/LICENSE.txt",
    "THIRD_PARTY_NOTICES.md": "docs/THIRD_PARTY_NOTICES.md",
}
REQUIRED_MARKERS = (
    "rclone 1.75.2",
    "Microsoft Fluent UI System Icons",
    "Qt 6",
    "LICENSE REVIEW INCOMPLETE",
    "Public release remains disabled",
)


def _file_bytes(path: Path) -> bytes:
    if not path.is_file() or path.is_symlink() or (
        hasattr(path, "is_junction") and path.is_junction()
    ):
        raise ValueError(f"Required license/notice file unavailable or linked: {path}")
    return path.read_bytes()


def git_blob_sha(data: bytes) -> str:
    return hashlib.sha1(
        b"blob " + str(len(data)).encode("ascii") + b"\0" + data
    ).hexdigest()


def verify_notices(source_dir: Path, notice_dir: Path,
                   rclone_version: str) -> int:
    if rclone_version != PINNED_RCLONE_VERSION:
        raise ValueError("Unreviewed rclone version: update original COPYING evidence first")
    if (not source_dir.is_dir() or source_dir.is_symlink() or
            not notice_dir.is_dir() or notice_dir.is_symlink()):
        raise ValueError("Source directory or notice directory unavailable/linked")
    source = {}
    for name, relative in LICENSE_FILES.items():
        data = _file_bytes(source_dir / relative)
        packaged = _file_bytes(notice_dir / name)
        if data != packaged:
            raise ValueError(f"Original notice missing or changed in package: {name}")
        source[name] = data
    if git_blob_sha(source["RCLONE_LICENSE.txt"]) != UPSTREAM_RCLONE_GIT_BLOB:
        raise ValueError("Vendored rclone COPYING does not match pinned upstream Git blob")
    if git_blob_sha(source["FLUENT_UI_SYSTEM_ICONS_LICENSE.txt"]) != FLUENT_GIT_BLOB:
        raise ValueError("Fluent license text changed: verify upstream and copyright")
    notice = source["THIRD_PARTY_NOTICES.md"].decode("utf-8")
    if any(marker not in notice for marker in REQUIRED_MARKERS):
        raise ValueError("Third-party notice missing a required attribution or warning")
    return len(LICENSE_FILES)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, required=True)
    parser.add_argument("--notice-dir", type=Path, required=True)
    parser.add_argument("--rclone-version", required=True)
    args = parser.parse_args()
    try:
        count = verify_notices(args.source_dir, args.notice_dir, args.rclone_version)
    except (OSError, ValueError, UnicodeError) as error:
        print(f"Third-party notice check FAILED: {error}", file=sys.stderr)
        return 1
    print(f"Third-party notice check passed: {count} exact source/package files; "
          "rclone upstream COPYING and Fluent Git blob pinned. "
          "Other licenses require review; release is not approved.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
