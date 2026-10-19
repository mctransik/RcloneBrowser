# Windows SBOM and third-party license review — 2026-10-19

> **Development only: implementation pending first GitHub CI run.**
> Not a complete or legally certified SBOM. The historical Firefox malware
> warning remains unresolved, and the release workflow remains disabled.

## Why inventory is needed

The previous Windows job produced a text-only SHA-256/signature inventory
for **26 EXE/DLL files**. That inventory omits non-executable payload
files (documentation, icons, INI, provenance manifests), and it does
not establish redistribution rights.

New generator: `scripts/generate_windows_sbom.py` produces a
**preliminary CycloneDX 1.6 JSON SBOM**, uploaded as a separate text-only
`windows-preliminary-cyclonedx-sbom` artifact after the signed-component
check. It deliberately does not alter or get packaged in the Windows ZIP.

The generator:
- Lists **every actual packaged file** (including the package hash
  manifest itself) as a CycloneDX `file` component with SHA-256 and
  package-relative name.
- Recomputes and verifies package hashes against
  `docs/PACKAGE_SHA256SUMS.txt`; rejects missing/extra paths, changed
  bytes, symlinks and duplicate case-insensitive pathnames.
- Cross-checks *all* packaged EXE/DLL paths, size and SHA-256 against
  the prior verified Windows component report. Refuses unreviewed
  binary families and unmatched executable files.
- Checks source commit, approved Qt and rclone versions against
  packaged build provenance, and validates application, Qt and
  rclone versions against the binary inventory.
- Adds logical entries for rclone, Qt, Microsoft VC++, ICU and Fluent
  UI System Icons embedded from the source repository. Clearly
  distinguishes source-level embedded icons from individually listed
  package files. MIT is recorded for the project's repository and
  rclone, and for the Fluent icon source license; **Qt and Microsoft
  library licenses are intentionally left undeclared**, not guessed.
- Marks the document as a **preliminary inventory**, explicitly not a
  malware verdict, distribution authorization or compliance review.

The generator has **11 synthetic-fixture tests**, including unexpected
files, payload tampering, manifest tampering, stale or incomplete
dependency inventory, unknown component family, wrong provenance
and version, and unknown art-license nonassertion. CI must still run
on an actual Windows package before claiming success.

## Licensing observations and blockers

| Distributable/embedded material | Evidence | Review work still required |
| --- | --- | --- |
| RcloneBrowser application and inherited project source | Root `LICENSE` includes MIT permission text and multiple upstream contributors | Verify original sources, attribution and inherited notices for all actual included code |
| Official rclone executable | [rclone's license](https://github.com/rclone/rclone/blob/master/docs/content/licence.md) says MIT; build verifies upstream PGP plus pinned archive SHA256 | Include upstream rclone license and copyright text with any distributed package; inspect transitive third-party notices |
| Qt 6.10.3 DLLs and plugins | [Qt 6.10 licensing](https://doc.qt.io/qt-6.10/licensing.html) lists LGPLv3/GPLv3/commercial choices, module-specific and third-party obligations | Determine exact modules' licenses; provide Qt license copies, third-party notices, complete corresponding sources/source offer, relinking rights and applicable LGPL requirements |
| 10 Microsoft Visual C++ runtime DLLs | Signed by Microsoft; [Microsoft redistribution](https://learn.microsoft.com/en-us/visualstudio/releases/2026/redistribution) lists permitted components under applicable license | Confirm each actual file is a valid unmodified redistributable from the properly licensed MSVC toolchain; record toolchain/version and license terms |
| `icuuc.dll` | Windows-runner catalog status Valid for Microsoft Windows; not proof of origin or embedded Authenticode | Independently determine actual binary's origin and its applicable redistribution terms; review transitive bundled dependencies |
| Fluent UI SVGs embedded in application resources | `src/icons/fluent/LICENSE.txt` contains MIT text and Microsoft attribution | Include required source notices in delivered product; check correspondence between embedded assets and source list |
| Cloud brand logos and remote PNGs | `icons/remotes/` packaged from repository | Audit per-logo authorship, redistribution/trademark rights, replacement possibilities |
| Documentation, PDF and screenshots | Packaged README, LICENSE, CHANGELOG and PDF | Review screenshot/privacy and rights; validate the packaged notices are adequate |

**Known packaging issue:** the current Windows distribution folder includes
a project `docs/LICENSE`, but the pipeline does **not** yet create a
complete Qt/rclone/Fluent third-party notices and LGPL-source compliance
bundle. Do **not** claim the existing package is fully ready for
redistribution because an SBOM file exists.

**Scope limitations:** SBOM file entries enumerate distributable bytes;
logical component rows are hand-classified references, not a full
transitive dependency graph. Dynamically downloaded or statically
compiled dependencies inside Qt and rclone, Linux AppImage components,
licensing alternatives and vulnerability status are not comprehensively
enumerated. Verify the official CycloneDX 1.6 schema in a future
dedicated pinned-validation gate; current tests exercise basic structure,
integrity and coverage only.

Sources: [CycloneDX 1.6 reference schema](https://github.com/CycloneDX/specification/blob/master/schema/bom-1.6.schema.json);
[Qt LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations);
[Microsoft VC++ distribution requirements](https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files).

Next: inspect the actual CI-produced JSON and then prioritize source
notices/Qt license-source delivery, asset provenance and Linux SBOM.
Nothing here re-enables a release or overrides browser protections.
