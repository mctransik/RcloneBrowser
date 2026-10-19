# License obligations and Windows component notice status — 2026-10-19

> **Development review; not legal advice and not a redistribution approval.**
> The release job remains disabled. Original Firefox download warning
> is unresolved. This report documents **open compliance tasks**, not a
> completed license-compliance certificate.

## Exact Windows components inventoried

The successful development workflow
[#39222019145](https://github.com/mctransik/RcloneBrowser/actions/runs/39222019145)
packaged 69 files. These included 26 Windows executables or DLLs:
13 Qt 6.10.3, 10 Microsoft VC++ runtime, 1 ICU, the application,
and rclone 1.75.2. The separate preliminary SBOM records their
paths, sizes, hashes and identities.

**Packaged Qt 6.10.3 files**, as observed in the actual Windows
package:

- Core/UI: `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Network.dll`,
  `Qt6Svg.dll`, `Qt6Widgets.dll`
- Image/icon plugins: `qt/plugins/iconengines/qsvgicon.dll`,
  `qt/plugins/imageformats/qgif.dll`, `qico.dll`, `qjpeg.dll`,
  and `qsvg.dll` in the same imageformats directory
- Platform/TLS: `qt/plugins/platforms/qwindows.dll`,
  `qt/plugins/tls/qcertonlybackend.dll`,
  `qt/plugins/tls/qschannelbackend.dll`

This is a file-level enumeration, **not proof that every
transitive Qt module, static library or third-party dependency is
fully covered**.

## Findings tied to official upstream information

| Component | Evidence | Before redistribution |
| --- | --- | --- |
| **Qt 6.10.3** | [Qt 6.10 licensing](https://doc.qt.io/qt-6.10/licensing.html), [Qt's LGPL obligations](https://www.qt.io/development/open-source-lgpl-obligations), and [Qt 6.10.3 third-party components](https://doc.qt.io/qt-6.10/licenses-used-in-qt.html) | Confirm actual licenses for **each included module/plugin** and embedded third party; give full applicable license and third-party notices; arrange corresponding Qt source code or a valid written offer under our control; preserve replace/relink freedoms and other LGPLv3 installation obligations; review selected rights and source distribution mechanism |
| **rclone 1.75.2** | PGP-verified pinned upstream release; exact-version [rclone COPYING](https://github.com/rclone/rclone/blob/v1.75.2/COPYING) states MIT | Include the **original upstream license and copyright notice**, not merely the inherited project LICENSE; audit any other bundled third-party notices |
| **Microsoft VC++ runtime** | Windows signature confirms Microsoft but not redistributability; [Microsoft redistribution terms](https://learn.microsoft.com/cpp/windows/redistributing-visual-cpp-files) limit individual DLL redistribution to licensed users under applicable license and REDIST list | Confirm precise Visual Studio/MSVC toolchain and eligibility of **all 10 actual DLLs**, as well as the build agent's licensed deployment terms; do not tag binaries as MIT |
| **ICU `icuuc.dll`** | Observed Microsoft runner **catalog** signature; no embedded signature asserted | Establish actual DLL provenance and applicable license/redistribution terms, independently from the runner's catalog |
| **Fluent UI System Icons** | [Microsoft Fluent UI System Icons repository](https://github.com/microsoft/fluentui-system-icons) and source file `src/icons/fluent/LICENSE.txt` attribute MIT to Microsoft Corporation | Include required Microsoft notice with distribution and ensure embedded resource correspondences; source LICENSE alone is not a packaged notice |
| **Cloud service logos** | Shipped as 34 PNG files under `icons/remotes/`; related README is the 35th icon-folder file | Review each asset's origin, license and trademark permissions; avoid claims of blanket MIT licensing |
| **Inherited project materials** | Root `LICENSE` and bundled PDF/README inherited from upstream forks | Verify attribution, documentation/screenshots, trademark use and any additional upstream notices |

## Immediate packaging gaps discovered

As of the real 2026-10-19 Windows artifact, the `docs/`
folder includes the root project's `LICENSE` only. It
**does not include rclone's exact-version COPYING, the Fluent
UI System Icons license, applicable Qt LGPL/GPL texts, or the
required Qt/third-party notices/source offer**. The package
must not be marked legally ready based on SBOM hashes alone.

**Next compliance work:** plan and implement a version-verified
third-party notice bundle as part of the Windows and Linux
packaging; explicitly review the Qt source-distribution and
replacement mechanism, the MSVC redistributable terms, ICU
provenance, each service logo, and the inherited docs and image
assets. Do not simply paste licensing names into the SBOM.

## Official CycloneDX schema validation

The new CI stage validates the preliminary Windows JSON against
the **upstream CycloneDX 1.6 schema** and its associated
SPDX and JSON Signature Format reference schemas. It downloads
those three files from commit
`ad83dae28b236b8760286399da4daba77592de86` of
[CycloneDX/specification](https://github.com/CycloneDX/specification)
and then verifies their original Git blob checksums from that
exact commit before any evaluation. Unknown external references
are rejected without network access by the schema validator.

The only new Python validation dependency is pinned to
`jsonschema==4.25.1`; **its transitive dependencies and wheel
hashes are not yet lockfile-controlled**. The official schema
data is under the [CycloneDX specification license (Apache-2.0)](https://github.com/CycloneDX/specification/blob/ad83dae28b236b8760286399da4daba77592de86/LICENSE);
the reference documents are read in CI, not incorporated into
the application's Windows ZIP.

**Validation status: verified in development CI run #39229425755.**
The official schema validator passed on the real Windows SBOM and all 7
schema regression tests passed in the later Windows validation step. CycloneDX conformance is also separate from
completeness of open-source license attribution.


## Workflow validation 2026-10-19

[Run #39229425755](https://github.com/mctransik/RcloneBrowser/actions/runs/39229425755) succeeded. The new pinned official CycloneDX 1.6 schema validation gate accepted the actual **74-component** JSON (69 exact Windows files plus 5 logical software entries); **all 7/7 schema tests** passed after the required Python dependency installation. The initial general 57-test Python suite skipped 3 schema tests before that installation; these were run later in the Windows schema stage. The SBOM and scanned Windows payload were separately downloaded: **69/69 paths and SHA-256 hashes matched**, and 68/68 package checksum-manifest entries matched. This shows data-format correctness and package correspondence, **not** completion of the third-party license duties listed above. No release was made.


## Development packaging update — exact rclone and Fluent notices (CI pending)

Commit 99afc2a added three files to the **proposed** Windows `docs/` payload and Linux AppImage support folder:

- `RCLONE_LICENSE.txt`: byte-for-byte copied from upstream rclone **v1.75.2** `COPYING` (Git blob `8c27c67fd0a12a1e18db983f19450abed435e689`). The original file is vendored in `third_party/licenses/`.
- `FLUENT_UI_SYSTEM_ICONS_LICENSE.txt`: byte-for-byte copied from repository `src/icons/fluent/LICENSE.txt` (Git blob `bc9c36b28f467f7006c4e5f1be4ca0a1a0b895f3`).
- `THIRD_PARTY_NOTICES.md`: names the origin/attributions and clearly says **LICENSE REVIEW INCOMPLETE**.

`scripts/verify_third_party_notices.py` rejects an unreviewed rclone version, altered pinned source texts, missing or altered packaged notices, and missing review warnings. Its **nine synthetic tests** cover valid packaging and failure paths. The strict Windows payload allowlist also requires all three additional documents, with two additional allowlist regression tests.

Expected next Windows payload/SBOM: **72 files**, not the previous 69; the real workflow has **not yet verified this build**. The previous report's 69-file counts remain correct for the earlier successful run.

**Still unresolved:** the complete Qt LGPL/GPL and third-party texts, source and relinking plan; Microsoft runtime redistribution eligibility; ICU provenance; cloud-logo author/trademark permissions; transitive third-party notice compliance. These additions do not grant distribution approval.
