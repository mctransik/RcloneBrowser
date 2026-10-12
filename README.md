<p align="center"><img src="docs/logo.png" width="96" alt="Rclone Browser logo"></p>

<h1 align="center">Rclone Browser – Security RC1 for Windows and Linux</h1>

> **Independent fork, not the official upstream project.** This repository is maintained under [mctransik/RcloneBrowser](https://github.com/mctransik/RcloneBrowser) and is based on [Jackpison's Rclone Explorer](https://github.com/Jackpison/RcloneExplorer) (formerly Rclone Browser), with earlier contributors credited below. It is not affiliated with or endorsed by the upstream maintainers. The original MIT copyright notices are retained.
>
> **Security RC1 — evaluation only.** This branch documents the earlier Rclone Browser interface and the [v4.0.2-rc.1 prerelease](https://github.com/mctransik/RcloneBrowser/releases/tag/v4.0.2-rc.1). It does **not** contain our new dashboard or two-pane cloud-management prototype. Test in a separate folder with disposable data; **do not use it for production files or backups**. The published release tag and download artifacts remain unchanged.
>
> **New project direction:** See the [main-branch project overview](https://github.com/mctransik/RcloneBrowser/tree/main) and [separate development branch](https://github.com/mctransik/RcloneBrowser/tree/feature/cloud-control-ui-mvp). Our future product name has **not** been selected.

<p align="center">This earlier rclone GUI can browse, transfer and sync files on supported cloud storage, powered by <a href="https://rclone.org/">rclone</a>.<br>Portable on Windows and available as an AppImage on Linux.</p>

<p align="center"><b><a href="https://github.com/mctransik/RcloneBrowser/releases/tag/v4.0.2-rc.1">Download this RC1 test release</a></b> · <a href="https://github.com/mctransik/RcloneBrowser/tree/main">Our project overview</a> · <a href="docs/Rclone-Browser-Guide.pdf">Legacy user guide (PDF)</a> · <a href="docs/remote-icons.md">Remote icons</a> · <a href="CHANGELOG.md">Changelog</a></p>

![Browsing a remote with tabs in dark mode](docs/browser.png)

## Features

Rclone Browser gives rclone a full graphical interface, so you can manage cloud files without the command line:

- **Modern design** in black and gold or warm light, with a one-click **light / dark** switch next to the app name
- **All your clouds in one place**: Google Drive, OneDrive, Dropbox, pCloud, S3, SFTP, encrypted remotes and every other rclone storage type, using your existing rclone configuration. Logos for 34 popular services are included; you can [add your own](docs/remote-icons.md).
- **File Explorer–style tabs**: open several remotes, or the same remote several times (Ctrl+T)
- **Icons and Details views**: extra large to small icons (Ctrl + mouse wheel), or a sortable list with name, size, modified date, type, extension and path
- Upload, download, create folders, rename, move and delete; drag and drop files from your file manager to upload
- **Transfers you can pause and resume**, with live progress, speed and time left
- Saved **tasks**: store a transfer once, run it again with one click (or as a dry run)
- Sidebar remotes sortable by name, type or your own order (drag and drop)
- Breadcrumb address bar, filter the current folder (Ctrl+F)
- Mount remotes as drives and stream media to a player such as [VLC](https://www.videolan.org/)
- **Built-in rclone updater**: rclone is bundled and kept up to date, every download checksum-verified

![Remotes on the Home page](docs/home.png)

![A running transfer](docs/transfers-dark.png)

![Details view in light mode](docs/details-light.png)

## Getting started

### Windows 10 / 11

1. For evaluation only, see `RcloneBrowser-4.0.2-windows-x64.zip` in the [v4.0.2-rc.1 prerelease](https://github.com/mctransik/RcloneBrowser/releases/tag/v4.0.2-rc.1). The release includes `SHA256SUMS.txt` for integrity checks.
2. Extract into a separate test folder and run `RcloneBrowser.exe` **only if your local security checks allow it**.
3. The executable is **not Authenticode-signed**. If Windows, Firefox or antivirus software warns about possible malware, **do not bypass or disable protections**; stop and investigate the warning first. A matching SHA-256 hash alone does not establish that an executable is safe.

Mounting remotes as drive letters needs [WinFsp](https://winfsp.dev/).

### Linux (x86_64)

1. For evaluation only, see `RcloneBrowser-4.0.2-linux-x86_64.AppImage` in the [v4.0.2-rc.1 prerelease](https://github.com/mctransik/RcloneBrowser/releases/tag/v4.0.2-rc.1).
2. Make it executable: `chmod +x RcloneBrowser-*.AppImage` (or *Properties → Allow executing as program*).
3. Run it.

Works on most distributions (Ubuntu, Fedora, Debian, Mint, Arch, openSUSE…). If it does not start, install FUSE 2 (`sudo apt install libfuse2t64` on Ubuntu 24.04+, `libfuse2` on older releases) or run it with `--appimage-extract-and-run`. Mounting needs FUSE 3 (`fuse3`).

Your existing rclone remotes appear automatically on both systems. If you have none yet, click **New remote** on the Home page (or **+** next to *Remotes* in the sidebar) to set one up with rclone's assistant. The **[user guide](docs/Rclone-Browser-Guide.pdf)** explains every feature in detail.

## Where things are stored

**Windows (portable):** `RcloneBrowser.ini` next to the exe switches portable mode on, and everything stays in the app folder:

| What | Where |
|---|---|
| rclone | `rclone\rclone.exe` (updated from inside the app) |
| rclone configuration | `%APPDATA%\rclone\rclone.conf` by default; copy it to `rclone\rclone.conf` to take your remotes with you |
| Settings / saved tasks | `RcloneBrowser.ini` / `tasks.bin` |
| Remote logos | `icons\remotes\<type>.png` (see [remote-icons.md](docs/remote-icons.md)) |
| Documentation | `docs\` |
| Qt plugins | `qt\plugins` (located via `qt.conf`) |

**Linux:** settings in `~/.config/rclone-browser/`, the rclone updated by the app in `~/.local/share/rclone-browser/rclone-browser/`, your rclone configuration in its usual place (`~/.config/rclone/rclone.conf`). To make the AppImage portable, create a folder named like the AppImage plus `.config` next to it (for example `RcloneBrowser-4.0.0-linux-x86_64.AppImage.config`); settings and an updated rclone then stay beside the AppImage. Your own remote logos go in `~/.local/share/rclone-browser/rclone-browser/icons/remotes/` (or `icons/remotes/` next to the AppImage in portable mode).

## Keyboard shortcuts

| Shortcut | Action |
|---|---|
| Ctrl+T / Ctrl+W | New tab / close tab (or middle-click the tab) |
| Ctrl+F | Filter the current folder |
| Ctrl+Shift+1 / Ctrl+Shift+2 | Icons view / Details view |
| Ctrl + mouse wheel | Bigger / smaller icons |
| Enter, double-click | Open folder |
| Alt+Up | Parent folder |
| F5 · F7 · F2 · Del | Refresh · New folder · Rename · Delete |
| Ctrl+U / Ctrl+D | Upload / Download |

## Security and evaluation status

- The RC1 release notes describe verification of rclone updates against **PGP-signed SHA-256 checksums**, including checks of the trusted release signing key. See the [RC1 release notes](https://github.com/mctransik/RcloneBrowser/releases/tag/v4.0.2-rc.1) for details.
- rclone remote credentials are managed through rclone's configuration. Treat `rclone.conf`, log files and diagnostic output as potentially sensitive; **do not publish secrets**.
- The RC1 artifacts were built through GitHub Actions and the release includes a `SHA256SUMS.txt` file. The release notes also state that build provenance attestations were published.
- **This is an unsigned evaluation prerelease, not a production-certified or malware-scanned application.** Hash checks, CI test results and provenance are useful integrity evidence but are not substitutes for independent security review or malware analysis.

## Feedback

GitHub Issues are **not currently enabled** in this fork. We will document a feedback channel when one is available. Do not publicly share cloud credentials, private configuration files or unredacted transfer logs.

## Building from source

Every push is built by GitHub Actions (`.github/workflows/update.yml`) for Windows and Linux; pushing a tag such as `v4.0.0` publishes a release with both packages.

**Windows:** Visual Studio 2022 or newer (C++ desktop development), CMake 3.21+, Ninja and Qt 6.10+ for MSVC 64-bit (including Qt SVG). From a *x64 Native Tools Command Prompt*:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.10.3\msvc2022_64
cmake --build build
ctest --test-dir build
```

**Linux:** a C++20 compiler, CMake, Ninja and Qt 6.10+ (Qt SVG included), e.g. on Ubuntu `sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-svg-dev`:

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/build/rclone-browser
```

The legacy user guide is generated with `python docs/guide/build_guide.py` (needs `pip install reportlab`). The `site/` directory contains **inherited upstream website material** and is not the official website of this independent fork; old upstream URLs and branding must not be reused as this project's identity.

## Credits and license

This independent development fork is based on [Jackpison / Rclone Explorer](https://github.com/Jackpison/RcloneExplorer) and the earlier [mmozeiko](https://github.com/mmozeiko/RcloneBrowser), [DinCahill](https://github.com/DinCahill/RcloneBrowser) and [kapitainsky](https://github.com/kapitainsky/RcloneBrowser) Rclone Browser projects. Interface icons include assets from [Microsoft Fluent UI System Icons](https://github.com/microsoft/fluentui-system-icons) under their own license terms.

The inherited source-code license is [MIT](LICENSE); original copyright and permission notices must remain intact. This RC1 branch is **not** the new independently branded cloud-management application.
