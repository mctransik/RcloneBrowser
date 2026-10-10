#!/usr/bin/env python3
"""Verify PGP-signed official rclone ZIP checksums before packaging.

Key bundled from rclone v1.75.2 docs/content/KEYS; fingerprint pinned below.
"""
import argparse
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

PINNED_SIGNER = "FBF737ECE9F8AB18604BD2AC93935E02FF3B54FA"
HASH_LINE = re.compile(rb"^([0-9a-fA-F]{64})[ \t]+\*?(\S+)[ \t]*$")


def verify_release(archive, sums, key, gpg="gpg", permitted=frozenset({PINNED_SIGNER})):
    archive, sums, key = map(Path, (archive, sums, key))
    if not all(f.is_file() for f in (archive, sums, key)):
        raise ValueError("Missing archive, SHA256SUMS or signing key")
    if not archive.stat().st_size or not 0 < sums.stat().st_size < 2000000:
        raise ValueError("Empty archive or invalid signed checksum size")
    if not sums.read_bytes().startswith(b"-----BEGIN PGP SIGNED MESSAGE-----"):
        raise ValueError("Unsigned SHA256SUMS; refusing to bundle rclone")
    # Fresh independent keyring; never use (or change) any user's keys.
    with tempfile.TemporaryDirectory(prefix="rclone-pgp-") as temp:
        common = [str(gpg), "--batch", "--no-tty", "--homedir", temp,
                  "--no-auto-key-retrieve"]
        key_import = subprocess.run(common + ["--import", str(key)],
                                    capture_output=True, timeout=30)
        if key_import.returncode:
            raise ValueError("Could not import bundled public key")
        result = subprocess.run(common + ["--status-fd", "2", "--decrypt", str(sums)],
                                capture_output=True, timeout=45)
        if result.returncode:
            raise ValueError("PGP signature of SHA256SUMS is invalid")
        signatures = [
            line.split()[2].upper()
            for line in result.stderr.decode("utf-8", "replace").splitlines()
            if line.startswith("[GNUPG:] VALIDSIG ") and len(line.split()) >= 3
        ]
        if len(signatures) != 1 or signatures[0] not in permitted:
            raise ValueError("Unexpected signer: not the pinned rclone release key")
        # Use ONLY the authenticated text emitted by gpg.
        matches = []
        for line in result.stdout.splitlines():
            m = HASH_LINE.fullmatch(line.strip())
            if m and m.group(2).decode("ascii", "replace") == archive.name:
                matches.append(m.group(1).decode("ascii").lower())
        if len(matches) != 1:
            raise ValueError("Signed SHA256SUMS must contain exactly one archive hash")
        digest = hashlib.sha256()
        with archive.open("rb") as f:
            for chunk in iter(lambda: f.read(1048576), b""):
                digest.update(chunk)
        if digest.hexdigest() != matches[0]:
            raise ValueError("ZIP checksum does not match PGP-signed SHA256SUMS")
        return signatures[0], digest.hexdigest()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--archive", required=True)
    p.add_argument("--sums", required=True)
    p.add_argument("--key", required=True)
    p.add_argument("--gpg", default="gpg")
    a = p.parse_args()
    try:
        signer, digest = verify_release(a.archive, a.sums, a.key, a.gpg)
    except (ValueError, OSError, subprocess.TimeoutExpired) as e:
        print(f"SECURITY ERROR: {e}", file=sys.stderr)
        return 1
    print(f"Verified PGP rclone signer: {signer}")
    print(f"Verified ZIP SHA-256: {digest}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
