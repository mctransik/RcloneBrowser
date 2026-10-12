# Cloud Management & Security — Development Project

**A cloud-management desktop application in development, powered by [rclone](https://rclone.org/).**

> **Project status: early development.** This repository is an independent development fork of [Jackpison's Rclone Explorer](https://github.com/Jackpison/RcloneExplorer) (formerly Rclone Browser). It is **not** the official Rclone Explorer project and is not affiliated with or endorsed by its maintainers. The final application name and visual identity have not been selected yet.

We are working towards a different kind of rclone interface: a **cloud management center** that combines storage information, file operations, transfer monitoring and transparent software-security information.

The application will continue using rclone as its file-transfer engine. Existing Qt 6 components provide a technical starting point while a distinct interface and navigation structure are developed.

## What we're building

| Area | Direction |
| --- | --- |
| **Dashboard** | Overview of configured storage locations, running jobs and rclone status |
| **Storage management** | Switchable **cards and list**, with capacity information where the backend supports it |
| **File manager** | A two-pane workspace for browsing local and remote storage side by side, alongside familiar tabbed browsing |
| **Transfers** | Clear source/destination information, progress, activity and error details |
| **Security center** | Installed rclone version, signed-update information and clearly distinguished verified/unverified states |
| **Appearance** | A new navy/teal design direction, with light and dark modes |

**Capacity information must be honest:** not all rclone backends support quota queries. Unsupported values should say “not available”, and encrypted remotes must not result in misleading double-counted storage totals.

**Safe defaults matter:** new two-pane copy actions are designed to require confirmation and avoid overwriting existing destination files. The interface and behavior still require further testing before general use.

## Current development status

The branches deliberately serve different purposes:

- **[main](../../tree/main)** — preserved baseline. The public project description has been updated, but the application code on this branch is not yet the new cloud-management UI.
- **[v4.0.2-rc.1](../../releases/tag/v4.0.2-rc.1)** — an earlier, tested *Rclone Browser* release candidate, including work on rclone update verification. **It does not contain the new dashboard or two-pane concept.**
- **[Cloud UI development branch](../../tree/feature/cloud-control-ui-mvp)** — prototype implementation of the dashboard, card/list storage overview, two-pane browser, transfer indicator and initial security-center screen.
- **[UI prototype pull request #3](../../pull/3)** — draft changes and build/test results. This work is not merged into the baseline.

### Security and verification

Our goal is to make security properties **inspectable rather than merely advertised**:

- Keep using rclone's existing functionality and its upstream release-signature verification path for updates.
- Show actual version and verification evidence when available.
- Clearly identify checks that were not performed, or for which no evidence is recorded.
- Preserve traceability to source code, build workflows and release artifacts.

A successful build, a checksum, a signature verification and a build attestation are different kinds of evidence. The UI must not present one as proof of another. The Security Center is still being developed; it does **not** yet provide a complete local verification history.

## Code signing policy (development draft)

This project is **not enrolled in SignPath Foundation** and does **not yet distribute Authenticode-signed project binaries**. Build checks, SHA-256 hashes and GitHub attestations are **not** equivalent to application signing or a malware scan.

We are documenting the prerequisites for a possible future application, without changing or releasing binaries:

- [Code signing policy — draft](docs/CODE_SIGNING_POLICY.md): proposed roles, manual approval and source-build verification; requirements **not yet enforced**.
- [Privacy and network behavior — draft](docs/PRIVACY.md): cloud traffic, local settings and the existing automatic rclone version-check behavior.
- [Third-party component review](docs/THIRD_PARTY_REVIEW.md): inherited attribution, dependencies, license checks and remaining release tasks.
- [Project identity and migration audit](docs/PROJECT_IDENTITY_AUDIT.md): inherited names and publisher metadata, Linux/Windows packaging, configuration migration and decisions to make before rebranding.
- [File-manager smoke-test checklist](docs/FILE_MANAGER_TEST_PLAN.md): focused local Back/Forward navigation, transfers and identity regression checks; results are still pending.

Eligibility for SignPath (particularly the upstream fork rules and public project reputation) is **not confirmed**. The final publisher name, verified contact, release policy and source/binary audits remain open.

## Testing and downloads

This is **not yet a new branded public release**. For the current prototype, review the [draft pull request](../../pull/3) and its linked GitHub Actions builds.

The [v4.0.2-rc.1 test release](../../releases/tag/v4.0.2-rc.1) belongs to the earlier UI and should not be mistaken for the planned cloud-management application. When testing new code, use a separate folder and disposable data; do not rely on an untested prototype for important files.

Issues and feedback: [GitHub Issues](../../issues).

## Building the project

The project is written in C++ with Qt 6 and uses CMake. Windows and Linux builds are checked by GitHub Actions.

For current build configuration and source code, see [src/CMakeLists.txt](src/CMakeLists.txt) and [the build workflow](.github/workflows/update.yml). Platform-specific packages and runtime prerequisites may change as development progresses.

## Origins, attribution and licensing

**This project is derived from existing open-source work, not written from scratch.** We want that history to remain clear:

- [Jackpison / Rclone Explorer](https://github.com/Jackpison/RcloneExplorer) — the modern Qt-based upstream project and direct parent of this GitHub fork.
- [kapitainsky / Rclone Browser](https://github.com/kapitainsky/RcloneBrowser) — an earlier Rclone Browser project.
- [Martins Mozeiko / Rclone Browser](https://github.com/mmozeiko/RcloneBrowser) and [DinCahill / Rclone Browser](https://github.com/DinCahill/RcloneBrowser) — earlier project contributions.
- [rclone](https://github.com/rclone/rclone) — the independent file-management and transfer engine.
- [Microsoft Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons) — interface icon resources under their own license terms.

The original copyright and permission notices are retained in the [MIT LICENSE](LICENSE). Third-party icons, trademarks, artwork and other assets may have additional conditions; these must be reviewed before introducing new branding or distributing a finished product.

**Our intention:** create a recognizably different cloud-management experience while continuing to credit the developers whose work made it possible.

---

*This README is a temporary project page. Name, logo, screenshots and documentation will be updated as the new application is implemented and verified.*
