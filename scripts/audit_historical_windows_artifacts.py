#!/usr/bin/env python3
"""Acquire and safely inspect six known historical Windows GitHub Actions ZIPs.

Only historical, repository-owned public artifact IDs and pinned ZIP hashes
are accepted. Never run ZIP members. Never upload ZIPs or extracted executables.
GitHub token is forwarded only to api.github.com, not the blob redirect.
"""
from __future__ import annotations

import hashlib
import csv
import os
from pathlib import Path, PurePosixPath
import shutil
import stat
import sys
import urllib.error
import urllib.parse
import urllib.request
import zipfile

REPO = "mctransik/RcloneBrowser"
# Run IDs are time-correlated candidates; not proof of the Firefox warning.
SAMPLES = (
    ("2026-10-10", 38064040344, 11674945118, "fe4035d8d92066f92dea0866906a0e5387258c4ca0ecae66d30fa613af2a3d19"),
    ("2026-10-10", 38067162852, 11674349574, "3f3698d2efa7a8c155b4ab1a89485347b1660bb6b2a775c23de60d00d8cf78da"),
    ("2026-10-11", 38122324539, 11696100220, "b9c5a216fb29f5c386bf5bbc09417dc4cedb95efec8b7077ea96025a98fbdc78"),
    ("2026-10-11", 38123394766, 11696436681, "826e9c62a3a2f37207c5caccfea39c4f5cafbeda5e30a11b12ad24c7da1a2627"),
    ("2026-10-11", 38126413180, 11697072636, "3c5070d58e5e05c7e6fd86b0fb6521ee67c85f8f049bee7b2c2c7f9d86611032"),
    ("2026-10-11", 38129020027, 11699016180, "55f47e0cd3dc62d535778a8695a70258eb05a1b99990c3aad3c566778049d21a"),
)

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None

def download(artifact_id: int, target: Path, token: str) -> None:
    url = f"https://api.github.com/repos/{REPO}/actions/artifacts/{artifact_id}/zip"
    req = urllib.request.Request(url, headers={
        "Authorization": f"Bearer {token}",
        "Accept": "application/vnd.github+json",
        "User-Agent": "RcloneBrowser-Historical-Security-Audit",
        "X-GitHub-Api-Version": "2022-11-28",
    })
    opener = urllib.request.build_opener(NoRedirect())
    try:
        response = opener.open(req, timeout=60)
    except urllib.error.HTTPError as exc:
        if exc.code not in (301, 302, 303, 307, 308):
            raise RuntimeError(f"GitHub artifact API returned HTTP {exc.code}") from exc
        location = exc.headers.get("Location", "")
        parsed = urllib.parse.urlsplit(location)
        host = (parsed.hostname or "").lower()
        if parsed.scheme != "https" or not (
            host.endswith(".blob.core.windows.net") or
            host.endswith(".githubusercontent.com")
        ):
            raise RuntimeError("GitHub redirected to an unexpected artifact host")
        # IMPORTANT: do not send the GitHub token to Azure/GitHub blob storage.
        response = urllib.request.urlopen(
            urllib.request.Request(location, headers={
                "User-Agent": "RcloneBrowser-Historical-Security-Audit"
            }), timeout=180
        )

    with response, target.open("wb") as output:
        shutil.copyfileobj(response, output, 1024 * 1024)
    if target.stat().st_size < 1024 or target.stat().st_size > 200 * 1024 * 1024:
        raise RuntimeError(f"Historical artifact has unexpected size: {target.stat().st_size}")

def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as reader:
        for chunk in iter(lambda: reader.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()

def extract_safely(source: Path, dest: Path) -> tuple[int, int]:
    # Limit decompression and reject traversal, absolute paths, symlinks,
    # duplicate destinations and device paths. No extracted file is executed.
    count, total = 0, 0
    seen: set[str] = set()
    with zipfile.ZipFile(source) as z:
        entries = z.infolist()
        if not (3 <= len(entries) <= 200):
            raise RuntimeError("Unexpected Windows package file count")
        for member in entries:
            name = member.filename.replace("\\", "/")
            parts = PurePosixPath(name).parts
            if (not parts or name.startswith("/") or
                any(p in ("", ".", "..") for p in parts) or
                ":" in parts[0] or "\x00" in name):
                raise RuntimeError("Unsafe member path in historical artifact")
            dest_name = "/".join(parts).casefold()
            if dest_name in seen:
                raise RuntimeError("Duplicate Windows package member")
            seen.add(dest_name)
            mode = member.external_attr >> 16
            if stat.S_ISLNK(mode) or (member.flag_bits & 0x1):
                raise RuntimeError("Symlink or encrypted archive member refused")
            count += 1
            total += member.file_size
            if member.file_size > 200 * 1024 * 1024 or total > 400 * 1024 * 1024:
                raise RuntimeError("Historical archive exceeds extraction limits")
            path = dest.joinpath(*parts)
            if member.is_dir():
                path.mkdir(parents=True, exist_ok=True)
            else:
                path.parent.mkdir(parents=True, exist_ok=True)
                with z.open(member) as source_member, path.open("xb") as output:
                    shutil.copyfileobj(source_member, output, 1024 * 1024)
                if path.stat().st_size != member.file_size:
                    raise RuntimeError("Historical archive member size mismatch")
    return count, total

def collect_windows_pe_hashes(root: Path) -> list[tuple[str, int, str]]:
    """Hash packaged Windows EXE/DLL bytes without executing them."""
    result: list[tuple[str, int, str]] = []
    for file in sorted(root.rglob("*")):
        if not file.is_file() or file.suffix.lower() not in (".exe", ".dll"):
            continue
        rel = file.relative_to(root).as_posix()
        result.append((rel, file.stat().st_size, sha256(file)))
    if not any(Path(p).name.lower() == "rclonebrowser.exe" for p, _, _ in result):
        raise RuntimeError("Historical package lacks RcloneBrowser.exe")
    if not any(p.lower().endswith("rclone/rclone.exe") for p, _, _ in result):
        raise RuntimeError("Historical package lacks bundled rclone.exe")
    if not any(p.lower().endswith(".dll") for p, _, _ in result):
        raise RuntimeError("Historical package lacks DLL dependencies")
    return result

def write_tsv(path: Path, header: tuple[str, ...], rows: list[tuple]) -> None:
    # Output only metadata, never binary samples.
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.writer(output, delimiter="\t", lineterminator="\n")
        writer.writerow(header)
        writer.writerows(rows)

def main() -> int:
    token = os.environ.get("GH_TOKEN", "").strip()
    if not token:
        raise RuntimeError("GitHub actions:read token not available")
    base = Path("historical-scan-inputs")
    base.mkdir(exist_ok=False)
    reports = Path("historical-reports")
    reports.mkdir(exist_ok=True)
    lines = [
        "Investigation of historical Windows Actions artifacts",
        "No archive or executable was executed or re-uploaded",
        "Run IDs are candidates based on signed URL validity, not proven flagged samples",
    ]
    report = reports / "historical-artifact-hashes.txt"
    pe_report = reports / "historical-windows-pe-sha256.tsv"
    unique_report = reports / "historical-windows-unique-pe-sha256.tsv"
    pe_rows: list[tuple[str, int, int, str, int, str]] = []
    unique: dict[str, dict[str, object]] = {}
    write_tsv(pe_report,
              ("date", "run_id", "artifact_id", "relative_path", "size_bytes", "sha256"),
              pe_rows)
    try:
        for date, run, artifact_id, expected in SAMPLES:
            archive = base / f"{run}-{artifact_id}.zip"
            print(f"Fetching run {run}, artifact {artifact_id} ({date}); no old signed URL used")
            download(artifact_id, archive, token)
            actual = sha256(archive)
            if actual != expected:
                raise RuntimeError(
                    f"Artifact SHA mismatch for run {run}: expected {expected}, got {actual}"
                )
            extracted = base / f"extracted-{run}"
            entries, expanded = extract_safely(archive, extracted)
            for path, size, digest in collect_windows_pe_hashes(extracted):
                pe_rows.append((date, run, artifact_id, path, size, digest))
                entry = unique.setdefault(digest, {"size": size, "paths": set(), "runs": set()})
                if entry["size"] != size:
                    raise RuntimeError("Same SHA-256 digest unexpectedly has different file sizes")
                entry["paths"].add(path)
                entry["runs"].add(run)
            write_tsv(pe_report,
                      ("date", "run_id", "artifact_id", "relative_path", "size_bytes", "sha256"),
                      pe_rows)
            lines.append(
                f"date={date} run={run} artifact_id={artifact_id} "
                f"sha256={actual} members={entries} extracted_bytes={expanded} "
                "hash_verified=true"
            )
            report.write_text("\n".join(lines) + "\n", encoding="utf-8")
            print(f"Hash verified, {entries} ZIP members. No member executed.")
        unique_rows = [
            (digest, entry["size"], len(entry["runs"]),
             ",".join(str(x) for x in sorted(entry["runs"])),
             ";".join(sorted(entry["paths"])))
            for digest, entry in sorted(unique.items())
        ]
        write_tsv(unique_report,
                  ("sha256", "size_bytes", "run_count", "run_ids", "relative_paths"),
                  unique_rows)
        lines.append(f"inner_exe_dll_entries={len(pe_rows)}")
        lines.append(f"unique_inner_exe_dll_sha256={len(unique_rows)}")
        print(f"All six outer ZIP hashes verified; {len(pe_rows)} EXE/DLL file instances; "
              f"{len(unique_rows)} unique EXE/DLL hashes. Nothing executed.")
        return 0
    finally:
        report.write_text("\n".join(lines) + "\n", encoding="utf-8")

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as err:
        print(f"Historical verification failed: {err}", file=sys.stderr)
        sys.exit(1)
