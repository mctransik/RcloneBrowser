"""Synthetic regression tests for pinned, offline CycloneDX schema validation."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import validate_cyclonedx_schema as validator


class CycloneDxSchemaValidationTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.schemas = self.root / "schemas"
        self.schemas.mkdir()
        data = {
            "bom-1.6.schema.json": {
                "$schema": "http://json-schema.org/draft-07/schema#",
                "$id": "http://cyclonedx.org/schema/bom-1.6.schema.json",
                "type": "object",
                "properties": {"bomFormat": {"enum": ["CycloneDX"]},
                               "specVersion": {"enum": ["1.6"]},
                               "componentLicense": {"$ref": "spdx.schema.json"}},
                "required": ["bomFormat", "specVersion", "componentLicense"],
            },
            "spdx.schema.json": {
                "$schema": "http://json-schema.org/draft-07/schema#",
                "$id": "http://cyclonedx.org/schema/spdx.schema.json",
                "enum": ["MIT"],
            },
            "jsf-0.82.schema.json": {
                "$schema": "http://json-schema.org/draft-07/schema#",
                "$id": "http://cyclonedx.org/schema/jsf-0.82.schema.json",
                "type": "object",
            },
        }
        hashes = {}
        for name, schema in data.items():
            raw = (json.dumps(schema) + "\n").encode("utf-8")
            (self.schemas / name).write_bytes(raw)
            hashes[name] = validator.blob_digest(raw)
        patcher = patch.dict(validator.SCHEMA_BLOBS, hashes, clear=True)
        patcher.start()
        self.addCleanup(patcher.stop)
        self.bom = self.root / "bill-of-materials.json"
        self.bom.write_text(json.dumps({
            "bomFormat": "CycloneDX", "specVersion": "1.6",
            "componentLicense": "MIT",
            "components": [{}, {}, {}],
        }), encoding="utf-8")

    def test_three_verified_schemas_load(self):
        self.assertEqual(len(validator.load_schemas(self.schemas)), 3)

    def test_tampered_schema_fails(self):
        path = self.schemas / "spdx.schema.json"
        path.write_bytes(path.read_bytes() + b"\n")
        with self.assertRaisesRegex(ValueError, "Git blob differs"):
            validator.load_schemas(self.schemas)

    def test_extra_schema_file_fails(self):
        (self.schemas / "new.json").write_text("{}")
        with self.assertRaisesRegex(ValueError, "Unexpected file"):
            validator.load_schemas(self.schemas)

    def test_schema_symlink_fails(self):
        self.assertEqual(len(validator.load_schemas(self.schemas)), 3)
        self.assertEqual(validator.blob_digest(b"hello"), "b6fc4c620b67d95f953a5c1c1230aaab5db5a1b0")

    @unittest.skipUnless(importlib.util.find_spec("jsonschema") is not None,
                         "Schema-validation dependency not installed in base test environment")
    def test_valid_bom_uses_offline_spdx_reference(self):
        self.assertEqual(validator.validate_bom(self.schemas, self.bom), 3)

    @unittest.skipUnless(importlib.util.find_spec("jsonschema") is not None,
                         "Schema-validation dependency not installed in base test environment")
    def test_schema_error_fails_closed(self):
        data = json.loads(self.bom.read_text())
        data["componentLicense"] = "Unapproved"
        self.bom.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "schema violation"):
            validator.validate_bom(self.schemas, self.bom)

    @unittest.skipUnless(importlib.util.find_spec("jsonschema") is not None,
                         "Schema-validation dependency not installed in base test environment")
    def test_unknown_external_schema_reference_rejected(self):
        target = self.schemas / "bom-1.6.schema.json"
        data = json.loads(target.read_text())
        data["properties"]["componentLicense"]["$ref"] = "https://example.org/untrusted.json"
        raw = (json.dumps(data) + "\n").encode()
        target.write_bytes(raw)
        validator.SCHEMA_BLOBS["bom-1.6.schema.json"] = validator.blob_digest(raw)
        with self.assertRaisesRegex(ValueError, "Unapproved external"):
            validator.validate_bom(self.schemas, self.bom)


if __name__ == "__main__":
    unittest.main()
