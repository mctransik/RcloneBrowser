# Privacy and network behavior — development draft

> **Draft, not a final legal privacy notice.** This document records behaviors visible in the current C++ application and build configuration. The new cloud-management interface is still under development; no complete runtime or dependency-wide privacy audit has been completed. Please do not treat unverified claims as guarantees.

## What the application does

This desktop application is a graphical interface for [rclone](https://rclone.org/). Its purpose is to let users configure and access their own local folders, NAS shares and remote cloud storage, initiate file operations, and monitor transfers.

**When the app accesses a cloud remote**, rclone communicates with the remote service selected/configured by the user. Depending on the operation and provider, file contents, names, paths, sizes, account identifiers, configuration identifiers and access credentials may be processed by rclone and transmitted to that provider according to the provider protocol. The privacy and retention practices of each service are governed by that service's own policies. This project does not operate the chosen cloud services.

**Local and NAS operations** access the paths and network shares chosen or navigated to by the user. For network shares, the operating system may handle authentication and network requests.

## Background and optional network activity

- **Automatic rclone version checks:** The current implementation can check official rclone version sources over HTTPS (including `downloads.rclone.org` and GitHub release endpoints). The setting `Settings/checkRcloneUpdates` defaults to enabled in the inspected code. Users can change that behavior in the application's rclone-update controls. Checking for an update is separate from installing it.
- **User-requested rclone updating:** An update action can initiate a download/update via rclone's built-in updater. The project aims to check upstream release signatures and checksums; this does not imply that the entire application is signed.
- **Remote listing and storage information:** Opening a configured cloud location or requesting its available capacity can cause remote API requests.
- **User-triggered transfers and features:** Copy, upload, download, remote management, streaming, mounting and share-link actions may cause network activity and/or launch external software.

A request to an external endpoint necessarily exposes connection metadata such as the device's public IP address to that endpoint. The app cannot control a cloud provider's independent logging, tracking or privacy practices.

## Local configuration and logs

- Application interface preferences and paths are kept in Qt settings; portable Windows packages can use `RcloneBrowser.ini` in their application directory. Settings may contain remote names and filesystem or NAS paths.
- Cloud account configuration and credentials are managed by rclone, commonly in an rclone configuration file. The GUI's inspected code prompts for an encrypted rclone config password when required and passes it to child rclone processes through the `RCLONE_CONFIG_PASS` environment variable. Users should protect their configuration and workstation.
- Job history, progress and error details can display file names, paths and remote names. Screenshots, exported output and logs shared with support may therefore contain sensitive information; redact them before sharing.
- The current application does not provide a project-hosted user account service. In the code paths inspected for this draft we did not identify a first-party analytics collection service; this is **not** a verified absence of all telemetry in bundled dependencies.

## Third parties and relevant policies

| Component/service | Purpose | Reference |
| --- | --- | --- |
| rclone | Provider protocols, remote configuration, file transfers and optional updates | https://rclone.org/privacy/ |
| Qt | Desktop UI and networking libraries | https://www.qt.io/terms-conditions/ |
| Configured cloud provider(s) | Storage and authentication under the user's own provider account | Consult each configured provider's privacy documentation |
| GitHub | Project website, source releases and some upstream update endpoints | https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement |
| rclone download server | Version information and upstream releases | https://downloads.rclone.org/ |

## Controls and remaining checks

- Use the rclone-update settings to decide whether the GUI should check rclone versions automatically.
- Configure only providers and remote folders you trust. rclone's `crypt` backend provides optional client-side encryption when correctly configured; not all rclone remotes are encrypted.
- Verify the actual settings-file location and the retention of transfer records on each supported operating system.
- Before claiming full compliance, audit all network paths and dependencies (including any inherited update-check functions), verify exactly what persists between launches, and publish appropriate user-facing privacy information for the finished application.
- Project contact and legally required publisher details will be added when the project identity and distribution model are finalized.

Last reviewed for the development branch: 2026-10-11. This document does not assert certification, security auditing, SignPath enrollment or complete GDPR compliance.
