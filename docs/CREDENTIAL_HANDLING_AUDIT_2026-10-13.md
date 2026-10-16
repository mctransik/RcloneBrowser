# Credential and secret handling — focused source audit (2026-10-13)

> **Partial audit only. No security certification or malware clearance.** Scope: development branch `feature/cloud-control-ui-mvp` after build-provenance changes. No real credentials were collected, accessed, or uploaded. Static source review only.

## Observed security-relevant data paths

| Area | Source | What was observed | Risk / remaining check |
| --- | --- | --- | --- |
| Encrypted rclone config password | `src/main_window.cpp`, `src/utils.cpp` | GUI prompts with password input; stores it in static `QString` for process lifetime; passes via `RCLONE_CONFIG_PASS` on selected rclone QProcess environments | No app-specific settings persistence seen. Still retained in memory; child inheritance and crash dumps need review |
| Proxy credentials | `src/preferences_dialog.cpp`, `src/main_window.cpp`, `src/main.cpp`, `src/rclone_updater.cpp` | Proxy URL strings saved in QSettings; exported into `HTTP_PROXY` / `HTTPS_PROXY` variables; updater parses URI username/password | A URL containing credentials can be written in clear text to configuration files and inherited by children |
| Saved transfer tasks | `src/job_options.cpp`, `src/list_of_job_options.cpp` | User-defined extra arguments serialize into a local QDataStream task file | May persist credential-bearing flags or URLs in plain text (binary container is not encryption) |
| Transfer clipboard | `src/job_widget.cpp` | Historically copied full command/args without confirmation | Development fix now asks for consent; explicit copy still includes all values |
| Process output | `src/job_widget.cpp`, `src/progress_dialog.cpp`, `src/stream_widget.cpp`, `src/mount_widget.cpp` | Logs are displayed from raw subprocess output | Credentials, signed links, full paths can enter screenshots/clipboard/other sharing; no comprehensive redaction |
| rclone command invocation | `src/main_window.cpp`, `src/remote_widget.cpp` | Most commands use QProcess argument lists, avoiding shell expansion of command-line values | Shell launch for config and configurable external players require separate review |

## Changes on this branch

- Corrected encrypted-config error detection at string index zero, allowing the GUI to prompt reliably when the hint begins the error output. An empty submitted password is not accepted for this encrypted-config prompt. The detection logic is isolated in `src/credential_safety.h` and has focused Qt tests for an index-zero hint, a later-position hint and unrelated errors. CI validation of the new tests is pending.
- Added a Cancel-default security confirmation before copying complete, unredacted rclone commands to the system clipboard. This does not prevent intentional copying, and clipboard contents may remain after quitting.

## Recommended remediation order

1. **Before beta / public release:** design secure storage and migration for proxy URLs and task extra-arguments, including existing installations; enforce appropriate file permissions and avoid writing credentials to new logs or diagnostics.
2. Define a conservative log-sharing policy; do not export raw logs by default. Review and test explicit token classes, credential-bearing URLs, query parameters, and subprocess stderr.
3. Review password lifetime and minimize secret-bearing child-process environments without breaking encrypted-config flows, uploads, mounts and manual rclone configuration.
4. Add automated tests (mock rclone + disposable credentials) and manual Windows GUI checks, including clipboard cancel/confirm behavior and password prompt at offset zero.

The current fixes are deliberately narrow. They do not prove confidentiality across the complete application and do not resolve the Firefox malware warning.

## 2026-10-16 — Best-effort masking of displayed rclone output (CI pending)

The code previously appended unfiltered output from rclone transfer, mount and stream processes to visible logs; the built-in updater likewise included up to 400 raw characters from failed `rclone selfupdate` stderr in a user-visible error. This creates a **potential** credential disclosure channel, not evidence of real data loss.

The development change adds `LogSafety::maskKnownCredentials` **only for UI presentation**: process-output lines in `JobWidget`, `MountWidget` and `StreamWidget`, and updater error details. It masks common HTTP(S)/SOCKS proxy URL userinfo, selected URL token/password query keys, Basic/Bearer Authorization headers, selected environment assignments, and explicit common CLI secret flags. Qt tests contain invented values only. rclone commands, PGP update checks and remote operations remain unchanged.

**Limitations:** This is *not* comprehensive secret detection or a guarantee that any output is safe to share. Vendor-specific signature query fields, arbitrary token formats, file/remote names and paths can remain sensitive. `ProgressDialog` receives arbitrarily split chunks rather than complete lines; it is **not yet covered** to avoid a partial-token leak masquerading as protection or unintentionally changing `outputAvailable` behavior. The app-wide proxy variables in `main.cpp`, process-lifetime encrypted config password, saved task extra flags, and portable settings still require separate investigation. No real credentials were read or submitted. The Firefox malware warning is unresolved; public release stays disabled.
