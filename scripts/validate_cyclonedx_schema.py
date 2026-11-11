#!/usr/bin/env python3
"""Offline CycloneDX 1.6 schema validation using three pinned upstream Git blobs.

Schema documents are downloaded separately by CI, verified byte-for-byte
against the immutable CycloneDX specification commit, and never used to
fetch additional documents from the internet.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

UPSTREAM_COMMIT = "ad83dae28b236b8760286399da4daba77592de86"
# Git blob object hashes for files in CycloneDX/specification at that commit.
SCHEMA_BLOBS = {
    "bom-1.6.schema.json": "b6c096a999d6ee9e408a9c3ae6c6227d6981c9ba",
    "spdx.schema.json": "566c4a32518c60778e55a56ccf71bf781320b279",
    "jsf-0.82.schema.json": "f46bfb1e52731ad1280123ff3e2bd29bd18d4bc2",
}
EXPECTED_ID = "http://cyclonedx.org/schema/"
ALLOWED_EXTERNAL = {
    "spdx.schema.json",
    "jsf-0.82.schema.json#/definitions/signature",
}


def blob_digest(data: bytes) -> str:
    """Git object SHA-1 of exact bytes, including its blob header."""
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()


def load_schemas(folder: Path) -> dict[str, dict]:
    if not folder.is_dir() or folder.is_symlink():
        raise ValueError("Schema directory missing or linked")
    documents = {}
    for name, required_sha in SCHEMA_BLOBS.items():
        path = folder / name
        if not path.is_file() or path.is_symlink():
            raise ValueError(f"Required schema file absent or linked: {name}")
        data = path.read_bytes()
        if blob_digest(data) != required_sha:
            raise ValueError(f"Official CycloneDX schema Git blob differs: {name}")
        schema = json.loads(data)
        if not isinstance(schema, dict) or schema.get("$id") != EXPECTED_ID + name:
            raise ValueError(f"Unexpected official schema identity: {name}")
        documents[name] = schema
    if set(path.name for path in folder.iterdir()) != set(SCHEMA_BLOBS):
        raise ValueError("Unexpected file in CycloneDX schemas directory")
    return documents


def schema_refs(node):
    if isinstance(node, dict):
        if "$ref" in node:
            yield node["$ref"]
        for value in node.values():
            yield from schema_refs(value)
    elif isinstance(node, list):
        for value in node:
            yield from schema_refs(value)


def validate_bom(folder: Path, bom_file: Path) -> int:
    schemas = load_schemas(folder)
    for name, schema in schemas.items():
        for ref in schema_refs(schema):
            if not isinstance(ref, str) or (
                not ref.startswith("#") and ref not in ALLOWED_EXTERNAL
            ):
                raise ValueError(f"Unapproved external schema reference in {name}: {ref}")
    if not bom_file.is_file() or bom_file.is_symlink():
        raise ValueError("SBOM JSON file absent or linked")
    bom = json.loads(bom_file.read_text(encoding="utf-8"))
    if not isinstance(bom, dict) or (bom.get("bomFormat"), bom.get("specVersion")) != ("CycloneDX", "1.6"):
        raise ValueError("Unexpected BOM format/specification version")

    # Import only when actually validating a BOM, so the checksum unit tests
    # remain stdlib-only and CI installs a pinned validator explicitly.
    from jsonschema import Draft7Validator
    from referencing import Registry, Resource

    registry = Registry().with_resources(
        (doc["$id"], Resource.from_contents(doc)) for doc in schemas.values()
    )
    Draft7Validator.check_schema(schemas["bom-1.6.schema.json"])
    validator = Draft7Validator(schemas["bom-1.6.schema.json"], registry=registry)
    errors = sorted(validator.iter_errors(bom), key=lambda e: repr(tuple(e.absolute_path)))
    if errors:
        first = errors[0]
        location = "/".join(str(part) for part in first.absolute_path) or "(root)"
        raise ValueError(f"CycloneDX 1.6 schema violation at {location}: {first.message}")
    return len(bom.get("components", []))


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--schemas-dir", required=True, type=Path)
    p.add_argument("--bom", required=True, type=Path)
    args = p.parse_args()
    try:
        count = validate_bom(args.schemas_dir, args.bom)
    except (OSError, ValueError, UnicodeError, ImportError, TypeError) as exc:
        print(f"Official CycloneDX 1.6 validation FAILED: {exc}", file=sys.stderr)
        return 1
    print(f"Official CycloneDX 1.6 schema validation passed: {count} components; "
          "three git-blob-pinned schemas; offline reference resolution. "
          "Not a license or security clearance.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
