# Third-party components and license review — working checklist

> **Review pending.** This is a planning inventory, not a completed software bill of materials (SBOM), compliance statement, license certification or declaration that every packaged asset may be redistributed. Verify the **exact versions, files and terms** in the real release package before publication or a SignPath application.

## Inherited software and authorship

This repository is an independent development fork of [Jackpison/RcloneExplorer](https://github.com/Jackpison/RcloneExplorer), with earlier origins in [kapitainsky/RcloneBrowser](https://github.com/kapitainsky/RcloneBrowser), [mmozeiko/RcloneBrowser](https://github.com/mmozeiko/RcloneBrowser) and other credited contributors. It is **not written from scratch**.

The repository [LICENSE](../LICENSE) retains the inherited MIT notice and permission terms. Its copyright holders must not be silently removed when creating a new product identity. GitHub currently reports the repository license as `Other / NOASSERTION` despite the MIT text being present; examine the notice and additional included terms rather than removing upstream authorship for cosmetic license detection.

## Preliminary inventory

| Component | Origin/use | License status / action required |
| --- | --- | --- |
| Project C++ source | This fork and inherited upstream contributors | MIT text present; verify every incorporated source file and required upstream attribution |
| **rclone** executable | Official upstream rclone, bundled by Windows and Linux CI | Upstream declares MIT; include required copyright/license texts and pin the exact package version for release review |
| **Qt 6** frameworks and plugins | Desktop application UI, SVG, networking and support libraries | Review the applicable **LGPL/GPL/commercial** terms, exact redistributable modules, plugins and dynamic-link/relinking obligations; provide required licenses/notices |
| **Microsoft VC++ runtime** | App-local runtime DLLs copied from a Microsoft Visual Studio redistributable directory | Verify Microsoft's redistribution terms and whether they qualify as system libraries under SignPath's OSS policy |
| **Fluent UI System Icons** | Icons embedded from the upstream assets | Icon package metadata indicates MIT; verify the actual asset set's license/NOTICE and preserve required notices |
| Additional bundled logos and images | `icons/remotes`, older artwork and package/application icons | Identify authors and applicable permissions **per asset**; do not assume all third-party logos or trademarks are MIT-licensed |
| **Inno Setup** notices / legacy installer assets | Inherited license file and possible packaging history | Check whether any installer or related binary is actually bundled before adding claims about the release |
| Inherited documentation, screenshots and website | `docs/`, `site/` and old Rclone Browser/Rclone Explorer branding | Confirm reproduction rights and replace stale parent-project links, metadata and screenshots when the new project identity is chosen |
| Linux AppImage packaging tools | `linuxdeploy` and Qt plugin used by CI | Check tool licenses, provenance and pinned downloaded binaries; include notices if the distributed artifact contains covered material |
| GitHub Actions CI tooling | Checkout, Python setup, Qt installer and release/attestation actions | Pinned action commits are present; review their transitive build dependencies, cached state, downloaded binaries and trust assumptions |

References: [rclone's license statement](https://github.com/rclone/rclone/blob/master/README.md), [Qt licensing](https://www.qt.io/licensing/), [Fluent UI SVG package](https://github.com/microsoft/fluentui-system-icons/blob/main/packages/svg-icons/package.json), and the [SignPath OSS conditions](https://signpath.org/terms.html).

## Before any signed or general release

- [ ] Create an inventory of the **actual** package contents with exact versions and source URLs (preferably SBOM), including Qt plugins, images and rclone.
- [ ] Record hashes of third-party release assets. Validate upstream signatures/checksums and review whether dependencies are downloaded dynamically at build time.
- [ ] Collect every required license text, copyright notice, modification notice and corresponding-source/relinking information for the binaries we distribute.
- [ ] Confirm the allowed use of the inherited branding, trademarks, icons and legacy PDF/website assets. The product's new branding must not imply official upstream endorsement.
- [ ] Resolve the upstream fork signing eligibility with the foundation. Do **not** sign externally authored upstream binaries as though we produced them.
- [ ] Verify stable product/version/publisher metadata in all signable Windows artifacts. Current resource metadata is inherited and transitional.
- [ ] Review the Windows packaging process and release files separately from Linux packaging; SignPath would sign only the approved Windows artifacts and formats.
- [ ] Add a human-reviewed release approval, an incident-response contact, and software signature verification evidence.
- [ ] Clarify the legal/project publisher and an appropriate privacy notice before public general release.

This document deliberately does not claim that a third-party license has been fully audited, or that SignPath will accept the project.
