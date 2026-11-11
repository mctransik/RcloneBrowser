#!/usr/bin/env python3
"""Compare the scanned ZIP digest with exact bytes retrieved for attestation.

Does not unpack, execute, publish or trust Windows binaries. A hash match
ensures the attestation subject is byte-for-byte the ZIP scanned by ClamAV.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import re
import sys

SHA_LINE = re.compile(
    r"\A([0-9a-fA-F]{64})[ \t]+[*]?"
    r"staging/(RcloneBrowser-[0-9]+\.[0-9]+\.[0-9]+-windows-x64[.]zip)\Z"
)


def verify_scanned_archive(checksums: Path, archive: Path) -> str:
    data = checksums.read_text(encoding="utf-8").strip()
    if "\n" in data or "\r" in data:
        raise ValueError("Expected exactly one Windows ZIP checksum line")
    match = SHA_LINE.fullmatch(data)
    if match is None:
        raise ValueError("Unexpected scanned Windows checksum record")
    expected, basename = match.groups()
    if archive.name != basename:
        raise ValueError("Scanned ZIP filename differs from downloaded ZIP filename")
    if not archive.is_file():
        raise FileNotFoundError("Windows ZIP to attest is absent")
    h = hashlib.sha256()
    with archive.open("rb") as reader:
        for block in iter(lambda: reader.read(1024 * 1024), b""):
            h.update(block)
    actual = h.hexdigest()
    if not hmac_compare(expected.lower(), actual):
        raise ValueError("Downloaded Windows ZIP digest differs from scanned ZIP")
    return actual


def hmac_compare(left: str, right: str) -> bool:
    # Constant-time compare is not needed to protect an archive hash, but avoids
    # accidental partial or case-variant matching.
    import hmac
    return hmac.compare_digest(left, right)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--checksum", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    args = parser.parse_args()
    try:
        digest = verify_scanned_archive(args.checksum, args.archive)
    except (OSError, ValueError) as exc:
        print(f"Scan-to-attestation ZIP verification FAILED: {exc}", file=sys.stderr)
        return 1
    print(f"Verified scanned Windows ZIP bytes: sha256:{digest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
