# Rclone Browser 4.0.2 — Security Release Candidate

**An independent, experimental fork of the rclone desktop GUI for Windows and Linux.**

> **RC1 is for evaluation only.** The [v4.0.2-rc.1 prerelease](https://github.com/mctransik/RcloneBrowser/releases/tag/v4.0.2-rc.1) is a security-focused test build. It has not been approved for production use. Try it in a separate directory, preferably with test remotes and disposable files. **Do not rely on it for important data or backups.**
>
> **Project independence and attribution:** This repository, [mctransik/RcloneBrowser](https://github.com/mctransik/RcloneBrowser), is an independent development fork of [Jackpison's Rclone Explorer](https://github.com/Jackpison/RcloneExplorer) (formerly Rclone Browser). It is not the official upstream project and is not endorsed by its maintainers. The software includes inherited code and other contributions acknowledged under [Credits and licensing](#credits-and-licensing).

## Why this release candidate exists

The purpose of this branch is to preserve an identifiable test version of the **earlier Rclone Browser application**, including work on verifying rclone downloads and tightening the build and update process. It is not a preview of our redesigned desktop interface.

The project is moving toward a broader cloud-management application with storage overviews, two-pane file operations, transfer management and a security page. That **separate, unfinished development** is documented in the [project overview on `main`](https://github.com/mctransik/RcloneBrowser/tree/main) and developed on [`feature/cloud-control-ui-mvp`](https://github.com/mctransik/RcloneBrowser/tree/feature/cloud-control-ui-mvp). The future product name has not been chosen.

We deliberately do not use inherited screenshots or artwork in this README to advertise the new project. **The RC1 application itself still shows the inherited Rclone Browser user interface.**

## What RC1 can do

This test version provides a graphical front end for [rclone](https://rclone.org/). The existing application supports these workflows:

| Area | Capabilities in this older application |
| --- | --- |
| Remote access | Display configured rclone remotes and navigate their folders |
| File operations | Upload, download and manage files and folders through the existing browser |
| Browsing | Work with multiple tabs and switch between file-list views |
| Transfers | Inspect transfer activity and use the application's existing task features |
| Remote configuration | Add or manage rclone remotes through the provided integration |
| Mounting | Use rclone's mount functionality when the required system components are installed |

Support for individual cloud services and operations depends on the selected rclone backend. Encrypted remotes depend on your own rclone configuration.

**Not included here:** our new dashboard, two-pane file manager, redesigned storage overview or new security-center interface. Those belong to the separate development work and must not be mistaken for RC1 features.

## What's being evaluated

Version **4.0.2** concentrates on the trustworthiness of rclone updates and release packaging. The [changelog](CHANGELOG.md) and [RC1 release notes](https://github.com/mctransik/RcloneBrowser/releases/tag/v4.0.2-rc.1) describe, among other items:

- Checking downloaded rclone archives against the upstream **PGP-signed checksum list**, including the expected signing-key identity.
- Rejecting failed signature, checksum and signer-verification cases in automated tests.
- Handling an already-current rclone version and passing proxy settings to the update process.
- Using verification during Windows and Linux release packaging, with GitHub Actions building and testing the packages.
- Publishing a checksum list with the release archives and configuring provenance attestations for tagged builds.

These are descriptions of the implemented/tested mechanisms, **not** a claim that the complete application has undergone a comprehensive security audit.

## Test packages

The published **[v4.0.2-rc.1](https://github.com/mctransik/RcloneBrowser/releases/tag/v4.0.2-rc.1)** release contains:

| Platform | Published file |
| --- | --- |
| Windows 10/11, x64 | `RcloneBrowser-4.0.2-windows-x64.zip` |
| Linux, x86_64 | `RcloneBrowser-4.0.2-linux-x86_64.AppImage` |
| Integrity information | `SHA256SUMS.txt` |

Read the release notes before testing. The provided SHA-256 hashes can help check whether a download matches the published archive; they do **not** establish that the program is harmless or independently reviewed.

### Windows evaluation

Use a separate folder rather than overwriting an existing installation. The portable application executable is named `RcloneBrowser.exe`. A `RcloneBrowser.ini` file beside it enables the existing portable-settings mode.

**The executable is not Authenticode-signed.** If Firefox, Windows or security software reports a possible malware threat, do **not** bypass the protection or disable the scanner. Stop and investigate the warning. An unsigned executable is not automatically malicious, but neither a successful build nor a matching checksum guarantees safety.

Mounting remotes as Windows drive letters may require [WinFsp](https://winfsp.dev/).

### Linux evaluation

The release provides an AppImage for x86_64. On supported distributions it can be marked executable and launched without a traditional installation. Depending on the distribution, AppImage execution may require compatible FUSE support. Mount operations have additional system requirements.

On either platform, **avoid pointing an experimental build at irreplaceable data**.

## Existing settings and sensitive data

RC1 retains the inherited application identifiers and configuration behavior. No product-name or settings-directory migration has taken place in this branch.

- rclone remote definitions are read from the rclone configuration selected by your setup; its standard locations and any portable overrides depend on your environment.
- On Windows, the shipped portable layout uses `RcloneBrowser.ini` for application settings, and the existing application supports saved tasks.
- On Linux, legacy application data uses the `rclone-browser` directory naming.
- `rclone.conf` can contain sensitive information. Keep it, any crypt passwords, and diagnostic logs private. Back up your original configuration before experimenting.

Do not copy settings blindly between test and production installations.

## Building and testing the source

The repository uses C++20, CMake and Qt, with the CI build using **Qt 6**. For Windows, use Visual Studio's MSVC C++ tools, Ninja, CMake and a compatible Qt 6 kit (including the SVG module). Linux builds require the corresponding Qt development components and a C++20-capable toolchain.

A basic local build uses:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Configure your Qt toolchain or `CMAKE_PREFIX_PATH` as needed. A successful source build or test run does not constitute an independent security assessment.

The [GitHub Actions workflow](.github/workflows/update.yml) contains the Windows and Linux build, packaging and release steps. The [changelog](CHANGELOG.md) records the changes in the RC1 source.

## Project status and feedback

The intended successor to this legacy interface is **still in development**. The stable baseline, the development work and this RC1 serve different purposes; changes are not silently merged between them.

GitHub Issues are currently **disabled** on this repository. There is no documented project-specific issue tracker yet. Never post credentials, `rclone.conf`, or raw logs containing private information in public discussions.

**About old materials:** Other files in this branch, including historic guides, source-level About dialogs and website assets, may still carry upstream Rclone Browser or Rclone Explorer wording. They are retained as part of the historical codebase and are **not** claims that our project is the official upstream release.

## Credits and licensing

This fork builds on the work of [Jackpison / Rclone Explorer](https://github.com/Jackpison/RcloneExplorer) and earlier Rclone Browser contributors, including [kapitainsky](https://github.com/kapitainsky/RcloneBrowser), [Martins Mozeiko](https://github.com/mmozeiko/RcloneBrowser) and [DinCahill](https://github.com/DinCahill/RcloneBrowser). The program uses [rclone](https://rclone.org/) as its file-transfer engine. Some interface resources derive from [Microsoft Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons).

The inherited repository [LICENSE](LICENSE) contains the MIT terms and historical copyright notices. Those notices are deliberately preserved. Bundled libraries, icons, service logos and other third-party components may have additional licensing or trademark requirements that must be evaluated independently.

---

**Documentation scope:** This README describes the `release-candidate/4.0.2-rc1` branch. Updating this branch's README does **not** rewrite the already-published `v4.0.2-rc.1` Git tag, the previously built archives or the README embedded in those archives.
