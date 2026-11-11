# Project identity, package metadata, and migration audit

> **Development working document (2026-10-11). No product name or publisher has been approved.**
>
> This document identifies exact inherited strings and technical constraints. It does **not** authorize a rebrand, change ownership, replace the upstream copyright notices, or prove SignPath eligibility. The project is an independent development fork of [Jackpison/RcloneExplorer](https://github.com/Jackpison/RcloneExplorer).

## Current identity

- Repository: [mctransik/RcloneBrowser](https://github.com/mctransik/RcloneBrowser), public fork of `Jackpison/RcloneExplorer`.
- Public README clearly labels the new cloud-management interface **early development**, not the upstream product.
- The public `v4.0.2-rc.1` release belongs to the **older** Rclone Browser interface. The cloud-control UI is only in `feature/cloud-control-ui-mvp`.
- Proposed product name, logo, publisher identity, privacy contact, release naming and desktop app ID: **not selected**.
- Main branch protection is configured separately; no release change or merge is proposed here.

## Exact source-level findings

| Category | File / location | Current behavior or inherited value | Plan after approving a product identity |
| --- | --- | --- | --- |
| Main window / navigation | `src/main_window.cpp`, `src/main_window_shell.cpp` | Rclone Browser title, navigation identity, and About screen | Use agreed product name and correct project repository; preserve upstream attributions separately |
| Upstream acknowledgement | `src/main_window.cpp` About box | Jackpison displayed as development/maintenance contact and linked issue tracker | Distinguish current project maintainer from historical upstream author; preserve accurate attribution |
| Windows metadata | `src/resources.rc` | `CompanyName`, `ProductName`, `FileDescription`, `OriginalFilename` and version metadata still reflect old product | Choose a **truthful publisher** (not a fabricated company or an unapproved SignPath publisher); match product and actual binary/version |
| Application IDs/settings | `src/main.cpp`, `src/utils.cpp` | Qt application/organization names and Linux settings paths use `rclone-browser`; portable mode uses `RcloneBrowser.ini` | Design a one-time migration or legacy fallback for existing settings and portable files before changing IDs |
| Single-instance behavior | `src/main.cpp` | Lock-file naming and user-facing message use old identity | Migrate carefully; test concurrent old/new versions and lock behavior |
| Windows packaging | `.github/workflows/update.yml` | Artifact names, ZIP folder, portable INI and self-test path depend on `RcloneBrowser` | Update consistently only as one reviewed packaging change after a name decision |
| Linux desktop identity | `packaging/linux/rclone-browser.desktop` | `Name`, `Exec`, `Icon`, `StartupWMClass` refer to old product | Change atomically with binary name, icon installation path and AppImage launch |
| Linux AppStream metadata | `packaging/linux/io.github.jackpison.RcloneBrowser.metainfo.xml` | Parent project's AppStream ID, homepage, developer and bug tracker | Use new unique app ID and the current project's own website/contact; keep upstream credit elsewhere |
| Linux build script | `.github/workflows/update.yml` | Old `rclone-browser` binary and AppStream filenames hard-coded | Change together with packaging files; verify built AppImage can launch |
| Inherited website | `site/index.html` | Parent developer, canonical URL, download links and screenshots | Do **not** publish as our website unchanged; build a new project-specific page when name and assets are ready |
| Third-party assets | `icons/remotes/`, `docs/`, `src/icon.*`, `src/icons/fluent/` | Mixed remote logos and inherited images; Fluent icons have a separate Microsoft MIT license file | Confirm asset rights and notices before rebranding or public general distribution |
| License detection | `LICENSE` | Retained historic copyright terms and MIT license; GitHub API reports `Other / NOASSERTION` | Review license layout and actual third-party notices without deleting upstream contributors for cosmetic detection |
| Feedback link | `README.md` and inherited UI/About references | Project links to GitHub Issues but repository's `has_issues` is currently **false** | Ask owner whether to enable Issues; avoid promising a feedback channel until enabled |
| Code signing | `docs/CODE_SIGNING_POLICY.md` | Signing policy is a draft; no active project Authenticode signing | Resolve publisher, release trust, upstream/fork eligibility and manual approval before applying |
| Version semantics | Root `VERSION` and `src/resources.rc` | `VERSION` is `4.0.2`; the Windows resources file defaults to `4.0.1` when no compiler override applies | Confirm version resource compilation and make displayed/file/package versions consistent |
| Security concerns | Current unsigned development artifacts | Previous Firefox malware warning has not been conclusively diagnosed | Investigate browser detection separately; a certificate is not a malware-free guarantee |

## Naming and publishing decisions to make with the owner

1. **Product name:** recognizable, not misleadingly close to upstream or an existing vendor; verify GitHub/domain and relevant trademark registrations before finalizing.
2. **Attribution line:** proposed visible subtitle: `Powered by rclone · Independent open-source project`, accompanied by a proper About/credits page listing inherited contributors.
3. **Publisher / legal identity:** choose an accurate public attribution. A GitHub handle is not a validated legal certificate publisher and should not be described as one.
4. **Program and package IDs:** avoid conflating display name, executable filename, `QCoreApplication::applicationName`, settings location and AppStream application ID.
5. **Migration:** test older portable INI and installed Qt settings and ensure no credentials/config paths are discarded. Never silently rewrite `rclone.conf` or `crypt` configuration.
6. **Website:** new URL, proper project metadata and updated screenshots; no implicit endorsement from upstream.
7. **Feedback channel:** enable and document project Issues, or name another concrete channel if GitHub Issues should remain disabled.

## Suggested controlled sequence (not yet implemented)

- **Phase A — identity decision:** agree on short product name and icon direction; verify naming conflicts before editing executable resources.
- **Phase B — technical migration design:** inventory stored settings and packaging identifiers, specify fallback and rollback rules, write tests before changing IDs.
- **Phase C — separate development-branch change:** update titles, About/credits, Windows version resources, Linux AppStream/desktop metadata, CI filenames and installation docs together.
- **Phase D — license/security gate:** complete third-party notices, audit release archives and investigate the Firefox detection. Avoid broad public distribution until sufficiently checked.
- **Phase E — manually approved candidate:** review all diffs, test update/install/migration and cloud/NAS/crypt transfers, and publish only after explicit human approval.

## Attribution rule

Never remove or overwrite the historical MIT copyright/permission notices to make the app appear original. A new name denotes the **new project identity and direction**, not ownership of third-party contributions. This requirement is independent of any future SignPath decision.

Related documents: [Code signing policy](CODE_SIGNING_POLICY.md), [Network/privacy draft](PRIVACY.md), [Third-party review](THIRD_PARTY_REVIEW.md).
