#!/usr/bin/env python3
"""Fail-closed inventory and sensitive-file check for the packaged Windows app.

Never executes packaged files; only inspects relative paths, settings and hashes.
This reduces accidental disclosure, not malware or arbitrary embedded secrets.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path, PurePosixPath
import re
import sys

ROOT_FILES = {"RcloneBrowser.exe", "RcloneBrowser.ini", "qt.conf"}
DOC_FILES = {
    "README.md", "LICENSE", "CHANGELOG.md", "Rclone-Browser-Guide.pdf",
    "BUILD_PROVENANCE.txt", "PACKAGE_SHA256SUMS.txt",
    "RCLONE_LICENSE.txt", "FLUENT_UI_SYSTEM_ICONS_LICENSE.txt",
    "THIRD_PARTY_NOTICES.md",
}
REQUIRED = {
    "RcloneBrowser.exe", "RcloneBrowser.ini", "qt.conf",
    "rclone/rclone.exe", "docs/README.md", "docs/LICENSE",
    "docs/CHANGELOG.md", "docs/Rclone-Browser-Guide.pdf",
    "docs/BUILD_PROVENANCE.txt", "docs/PACKAGE_SHA256SUMS.txt",
    "docs/RCLONE_LICENSE.txt", "docs/FLUENT_UI_SYSTEM_ICONS_LICENSE.txt",
    "docs/THIRD_PARTY_NOTICES.md",
}
SENSITIVE_TEXT = [
    re.compile(rb"-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----"),
    re.compile(rb"(?<![A-Za-z0-9_])github_pat_[A-Za-z0-9_]{40,}"),
    re.compile(rb"(?<![A-Za-z0-9_])gh[pousr]_[A-Za-z0-9_]{25,}"),
    re.compile(rb"(?<![A-Za-z0-9])AKIA[0-9A-Z]{16}(?![0-9A-Z])"),
    re.compile(rb"(?<![A-Za-z0-9])xox[baprs]-[A-Za-z0-9-]{20,}"),
]
MAX_TEXT_BYTES = 2 * 1024 * 1024


def permitted_file(name: str) -> bool:
    parts = PurePosixPath(name).parts
    if len(parts) == 1:
        return name in ROOT_FILES or name.lower().endswith(".dll")
    if len(parts) == 2 and parts[0] == "rclone":
        return parts[1] == "rclone.exe"
    if len(parts) == 2 and parts[0] == "docs":
        return parts[1] in DOC_FILES
    if len(parts) == 3 and parts[:2] == ("icons", "remotes"):
        return (parts[2] == "README.md" or
                (parts[2].lower().endswith(".png") and len(parts[2]) > 4))
    if len(parts) == 4 and parts[:2] == ("qt", "plugins"):
        return (bool(re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]*", parts[2]))
                and parts[3].lower().endswith(".dll"))
    return False


def permitted_dir(name: str) -> bool:
    parts = PurePosixPath(name).parts
    return name in {"rclone", "docs", "icons", "icons/remotes", "qt",
                    "qt/plugins"} or (
                        len(parts) == 3 and parts[:2] == ("qt", "plugins")
                        and bool(re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]*",
                                              parts[2]))
                    )


def not_regular(path: Path) -> bool:
    return (path.is_symlink() or
            (hasattr(path, "is_junction") and path.is_junction()) or
            not (path.is_file() or path.is_dir()))


def hash_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for data in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(data)
    return h.hexdigest()


def audit_package(root: Path) -> int:
    if not root.is_dir() or not_regular(root):
        raise ValueError("Package root is missing, linked or not a directory")
    files: dict[str, Path] = {}
    for folder, dirs, names in os.walk(root, followlinks=False):
        parent = Path(folder)
        for dirname in dirs:
            path = parent / dirname
            rel = path.relative_to(root).as_posix()
            if not permitted_dir(rel) or not_regular(path):
                raise ValueError(f"Unexpected or linked package directory: {rel}")
        for filename in names:
            path = parent / filename
            rel = path.relative_to(root).as_posix()
            if not permitted_file(rel) or not path.is_file() or not_regular(path):
                raise ValueError(f"Unexpected or linked package file: {rel}")
            if rel.casefold() in (existing.casefold() for existing in files):
                raise ValueError("Duplicate case-insensitive package pathname")
            files[rel] = path

    missing = sorted(REQUIRED - files.keys())
    if missing:
        raise ValueError("Missing required package files: " + ", ".join(missing))

    expected_ini = "[Settings]\nrclone=rclone/rclone.exe\ncheckRcloneUpdates=true"
    actual_ini = files["RcloneBrowser.ini"].read_text(
        encoding="utf-8-sig").strip().replace("\r\n", "\n")
    if actual_ini != expected_ini:
        raise ValueError("Unexpected portable settings (possible secret leak)")
    actual_qt = files["qt.conf"].read_text(
        encoding="utf-8-sig").strip().replace("\r\n", "\n")
    if actual_qt != "[Paths]\nPlugins = qt/plugins":
        raise ValueError("Unexpected Qt configuration")

    # The text scanner deliberately targets high-confidence secrets only;
    # it cannot detect custom tokens or secrets compiled into executables.
    for name, path in files.items():
        if not (name.startswith("docs/") or
                name in {"RcloneBrowser.ini", "qt.conf"}):
            continue
        if path.suffix.lower() == ".pdf":
            continue  # Binary PDF, not a reliably text-searchable format.
        if path.stat().st_size > MAX_TEXT_BYTES:
            raise ValueError(f"Unexpected oversized package text file: {name}")
        data = path.read_bytes()
        if any(pattern.search(data) for pattern in SENSITIVE_TEXT):
            raise ValueError(f"Potential private credential in package: {name}")

    manifest = files["docs/PACKAGE_SHA256SUMS.txt"]
    expected_lines = {
        f"{hash_file(path)}  {name}"
        for name, path in files.items()
        if name != "docs/PACKAGE_SHA256SUMS.txt"
    }
    manifest_lines = manifest.read_text(encoding="ascii").splitlines()
    if len(manifest_lines) != len(expected_lines) or set(manifest_lines) != expected_lines:
        raise ValueError("Payload checksum manifest incomplete or mismatched")
    return len(files)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-dir", required=True, type=Path)
    args = parser.parse_args()
    try:
        count = audit_package(args.package_dir)
    except (OSError, UnicodeError, ValueError) as exc:
        print(f"Windows package inventory FAILED: {exc}", file=sys.stderr)
        return 1
    print(f"Windows package inventory passed: {count} expected-shape files; "
          "no high-confidence text credential markers.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
