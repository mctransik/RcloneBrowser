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

**Validation status: pending first workflow execution** on
this change. No schema conformance can be claimed from earlier
successful runs. CycloneDX conformance is also separate from
completeness of open-source license attribution.
