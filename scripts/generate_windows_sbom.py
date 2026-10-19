#!/usr/bin/env python3
"""Create a CycloneDX 1.6 Windows payload SBOM from verified package bytes.

Inventories every packaged file, but does not determine legal compliance,
vulnerability status, or transitive libraries hidden inside static binaries.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import sys
import uuid

SHA256 = re.compile(r"[0-9a-f]{64}\Z")
COMMIT = re.compile(r"[0-9a-f]{40}\Z")


def digest_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for data in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(data)
    return h.hexdigest()


def payload_files(root: Path) -> dict[str, Path]:
    if not root.is_dir() or root.is_symlink():
        raise ValueError("Missing or linked payload directory")
    files = {}
    seen = set()
    for path in root.rglob("*"):
        rel = path.relative_to(root).as_posix()
        if path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction()):
            raise ValueError(f"Linked package entry: {rel}")
        if path.is_dir():
            continue
        if not path.is_file():
            raise ValueError(f"Non-regular package entry: {rel}")
        if rel.casefold() in seen:
            raise ValueError(f"Case-insensitive filename collision: {rel}")
        seen.add(rel.casefold())
        files[rel] = path
    return files


def check_manifest(root: Path, files: dict[str, Path]) -> dict[str, str]:
    name = "docs/PACKAGE_SHA256SUMS.txt"
    if name not in files:
        raise ValueError("Missing packaged SHA-256 manifest")
    lines = files[name].read_text(encoding="ascii").splitlines()
    expected = {}
    for line in lines:
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        if match is None:
            raise ValueError("Invalid package checksum line")
        value, relative = match.groups()
        parts = PurePosixPath(relative)
        if (parts.is_absolute() or ".." in parts.parts or "\\" in relative or
            relative in expected or relative == name):
            raise ValueError("Invalid or repeated package checksum path")
        expected[relative] = value
    if set(expected) != set(files) - {name}:
        raise ValueError("Checksum manifest does not cover exact package payload")
    actual = {}
    for relative, path in files.items():
        value = digest_file(path)
        if relative in expected and expected[relative] != value:
            raise ValueError(f"Packaged file differs from manifest: {relative}")
        actual[relative] = value
    return actual


def check_provenance(files: dict[str, Path], commit: str,
                     qt: str, rclone: str) -> None:
    name = "docs/BUILD_PROVENANCE.txt"
    if name not in files:
        raise ValueError("Missing build provenance")
    pairs = {}
    for line in files[name].read_text(encoding="utf-8-sig").splitlines():
        if "=" not in line:
            raise ValueError("Malformed provenance line")
        key, value = line.split("=", 1)
        if key in pairs:
            raise ValueError("Repeated provenance field")
        pairs[key] = value
    for key, value in [("source_commit", commit), ("qt_version", qt),
                       ("rclone_version", rclone)]:
        if pairs.get(key) != value:
            raise ValueError(f"Unexpected provenance {key}")


def package_family(path: str, binary: dict) -> str:
    if binary:
        return binary["family"]
    if path.startswith("icons/remotes/"):
        return "remote_logos_and_artwork_license_review"
    if path.startswith("docs/"):
        return "project_documentation_or_package_metadata"
    return "application_support"


def generate_bom(root: Path, inventory_path: Path, *,
                 commit: str, app: str, qt: str, rclone: str) -> dict:
    if not COMMIT.fullmatch(commit):
        raise ValueError("Source commit must be a full lowercase SHA")
    files = payload_files(root)
    hashes = check_manifest(root, files)
    check_provenance(files, commit, qt, rclone)
    inventory = json.loads(inventory_path.read_text(encoding="utf-8"))
    if inventory.get("schema") != "rclonebrowser.windows-dependencies.v1":
        raise ValueError("Unsupported Windows binary inventory format")
    if inventory.get("versions") != {"application": app, "qt": qt,
                                     "rclone": rclone.lstrip("v")}:
        raise ValueError("Binary inventory versions do not match build")
    binary_files = {p for p in files if p.lower().endswith((".exe", ".dll"))}
    rows = inventory.get("files")
    if not isinstance(rows, list) or inventory.get("file_count") != len(rows):
        raise ValueError("Malformed Windows binary inventory")
    binary = {}
    for entry in rows:
        path = entry.get("path")
        if path in binary or path not in binary_files:
            raise ValueError("Duplicated/unexpected binary inventory member")
        if entry.get("sha256") != hashes[path] or entry.get("size_bytes") != files[path].stat().st_size:
            raise ValueError(f"Outdated or altered binary inventory: {path}")
        if entry.get("family") not in {
            "project_application", "upstream_rclone", "qt_framework",
            "microsoft_vc_runtime", "icu_runner_catalog_signature",
        }:
            raise ValueError(f"Unreviewed dependency family: {path}")
        binary[path] = entry
    if set(binary) != binary_files:
        raise ValueError("Binary inventory does not cover all packaged EXE/DLL")
    logical = [
        {"type": "library", "bom-ref": "logical:rclone",
         "name": "rclone", "version": rclone.lstrip("v"),
         "licenses": [{"license": {"id": "MIT"}}],
         "properties": [{"name": "rclonebrowser:license-verification",
                         "value": "upstream_MIT_license; notices_and_transitive_packages_require_review"}]},
        {"type": "framework", "bom-ref": "logical:Qt",
         "name": "Qt", "version": qt,
         "properties": [{"name": "rclonebrowser:license-verification",
                         "value": "LGPL/GPL/commercial_options; per-module_obligations_not_yet_confirmed"}]},
        {"type": "library", "bom-ref": "logical:MSVC",
         "name": "Microsoft Visual C++ runtime",
         "properties": [{"name": "rclonebrowser:license-verification",
                         "value": "Microsoft_redistribution_terms_require_review"}]},
        {"type": "library", "bom-ref": "logical:ICU",
         "name": "ICU Unicode runtime",
         "properties": [{"name": "rclonebrowser:license-verification",
                         "value": "origin_and_license_notice_require_review"}]},
        {"type": "data", "bom-ref": "logical:fluent-icons",
         "name": "Fluent UI System Icons (compiled resources)",
         "licenses": [{"license": {"id": "MIT"}}],
         "properties": [{"name": "rclonebrowser:evidence",
                         "value": "repository src/icons/fluent/LICENSE.txt; embedded assets not separately enumerated"}]},
    ]
    components = []
    for relative in sorted(files):
        row = binary.get(relative)
        component = {
            "type": "file", "bom-ref": "file:" + relative, "name": relative,
            "hashes": [{"alg": "SHA-256", "content": hashes[relative]}],
            "properties": [
                {"name": "rclonebrowser:packaged-size-bytes",
                 "value": str(files[relative].stat().st_size)},
                {"name": "rclonebrowser:family",
                 "value": package_family(relative, row or {})},
            ],
        }
        if row:
            component["properties"].extend([
                {"name": "rclonebrowser:windows-version-metadata",
                 "value": str(row.get("pe_file_version", ""))},
                {"name": "rclonebrowser:windows-signature-status",
                 "value": str(row.get("signature_status", ""))},
                {"name": "rclonebrowser:windows-signature-type",
                 "value": str(row.get("signature_type", ""))},
                {"name": "rclonebrowser:windows-signer-subject",
                 "value": str(row.get("signer_subject", ""))},
            ])
        components.append(component)
    serial = uuid.uuid5(uuid.NAMESPACE_URL, f"rclonebrowser:{commit}:{hashes['docs/PACKAGE_SHA256SUMS.txt']}")
    return {
        "bomFormat": "CycloneDX", "specVersion": "1.6",
        "serialNumber": "urn:uuid:" + str(serial), "version": 1,
        "metadata": {
            "timestamp": datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z"),
            "component": {
                "type": "application", "bom-ref": "application:RcloneBrowser",
                "name": "RcloneBrowser", "version": app,
                "licenses": [{"license": {"id": "MIT"}}],
                "externalReferences": [{"type": "vcs",
                     "url": "https://github.com/mctransik/RcloneBrowser/tree/" + commit}],
            },
            "properties": [
                {"name": "rclonebrowser:source-commit", "value": commit},
                {"name": "rclonebrowser:package-file-count", "value": str(len(files))},
                {"name": "rclonebrowser:review-status",
                 "value": "preliminary_windows_inventory; licenses_and_transitive_dependencies_incomplete"},
                {"name": "rclonebrowser:security-interpretation",
                 "value": "not_malware_clearance_or_release_approval"},
            ],
        },
        "components": logical + components,
        "dependencies": [{"ref": "application:RcloneBrowser",
                          "dependsOn": [p["bom-ref"] for p in logical]}],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--package-dir", type=Path, required=True)
    parser.add_argument("--binary-inventory", type=Path, required=True)
    parser.add_argument("--source-commit", required=True)
    parser.add_argument("--app-version", required=True)
    parser.add_argument("--qt-version", required=True)
    parser.add_argument("--rclone-version", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = generate_bom(args.package_dir, args.binary_inventory,
                              commit=args.source_commit,
                              app=args.app_version, qt=args.qt_version,
                              rclone=args.rclone_version)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(result, indent=2, ensure_ascii=True) + "\n",
                               encoding="utf-8")
    except (OSError, ValueError, KeyError, TypeError, UnicodeError) as error:
        print(f"Windows SBOM generation FAILED: {error}", file=sys.stderr)
        return 1
    count = sum(c["type"] == "file" for c in result["components"])
    print(f"CycloneDX 1.6 Windows inventory created: {count} exact payload files, "
          "5 separately identified logical dependencies; license review incomplete.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
