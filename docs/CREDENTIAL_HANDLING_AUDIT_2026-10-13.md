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

### First CI regression and fix (2026-10-16; rerun verified)

[Run #38881049730](https://github.com/mctransik/RcloneBrowser/actions/runs/38881049730) compiled successfully on both Windows and Linux but passed only **5/6 CTest suites**; `log_safety` failed because the named-assignment masking expression consumed `&v=2` and further URL parameters after an already-masked query token. This was a **non-secret content integrity defect** detected by the new regression test. The assignment pattern now stops at URL query delimiters (`&`, `?`, `#`), keeping non-secret fields and separately masking subsequent secret fields. Added exact-output tests for query-field ordering and chained assignments. New Windows/Linux CI, Windows malware scanning and provenance checks must pass before claiming this fix validated. **No publication**.

### Verified second CI run (2026-10-16)

[Run #38885704340](https://github.com/mctransik/RcloneBrowser/actions/runs/38885704340) completed successfully on development commit `4109b2ad2f53035269543bcc57e33c9d2cc76846`. All **6/6 CTest suites passed** on Windows and Linux, including the updated `log_safety` suite with eight synthetic-credential test methods. No runtime Windows GUI injection testing has yet been performed.

Windows Authenticode audit recorded **26 EXE/DLL files, 24 valid signatures, 2 unsigned files, 0 abnormal results**. ClamAV scanned **70 current files (0 detections)** and **408 historical files (0 detections)** using 3,628,169 signatures. The exact scanned Windows archive has independently verified SHA-256 `ccb943daa371132a888743678ec1c6aa632dc077080cc8a25fe0b318c3ec01f7` and [GitHub provenance attestation #56619377](https://github.com/mctransik/RcloneBrowser/attestations/56619377); CI verified the attestation's source identity. The release job was skipped by design.

These checks demonstrate the test/build and scan pipeline passed; they cannot certify malware absence, all-log confidentiality or resolved Firefox download classification. The public release gate remains disabled.

## 2026-10-16 — App-specific proxy variables scoped to rclone children (CI pending)

Previously, the startup function installed the saved HTTP(S) proxy URLs (which could include usernames/passwords) in GUI-wide environment variables with `qputenv`. Every unrelated child program, such as a configurable video player, could inherit them. The source now removes those global changes. After the original protected-proxy load and startup fail-closed validation, settings are held in session memory using `SetRcloneProxy`. Each regular rclone process uses the existing `UseRclonePassword(QProcess*)` hook, which now places the chosen proxy variables only in that particular rclone process environment, alongside `RCLONE_CONFIG_PASS` when needed. Saving preferences updates newly launched rclone processes; disabling the custom proxy clears the app-managed values. Six regression tests with invented credentials cover app-only scope, case variants, inherited user variables, reconfiguration, and config-password handling.

The separate signed rclone updater already explicitly sets its own network/child proxy, and this change leaves those semantics intact. **Limits:** User/system proxy variables existing *before* launch can still be inherited, rclone children still receive necessary secrets, and secrets remain in application memory. This does not solve arbitrary log redaction, insecure task option persistence, or historical Firefox malware classification. Release remains disabled.

### Verified CI results — 16 October 2026

[Run #38925760533](https://github.com/mctransik/RcloneBrowser/actions/runs/38925760533) completed **successfully** for commit `b7ba7917108f77f7879cc97466196d86fc740ba9`. Windows and Linux each passed **7/7 CTest suites**, including the new `proxy_process_environment` suite containing six synthetic-data test methods. Windows Authenticode audit: **26 EXE/DLL files, 24 valid signatures, 2 unsigned, 0 abnormal**. Current Windows ClamAV scan: **70 files, zero detections**; historical Windows scan: **408 files, zero detections**; signature database: **3,628,169 known signatures**. The exact scanned Windows archive SHA-256 `90bfce6a402d51661fe58eea0ac8df6b0e34fb93d5c743d1aaa6754c709eb507` had a [GitHub provenance attestation #56715991](https://github.com/mctransik/RcloneBrowser/attestations/56715991) independently verified by CI. The public release job remained disabled and was skipped.

**Qualification:** Test and scanning success is not a security certification or proof of no malware or no credential leakage. Six helper tests did not simulate every GUI/proxy/credential-manager integration path; a manual Windows network/proxy test with disposable credentials is still recommended. Inherited system proxy environment variables, local process inspection, runtime memory, additional logfile/CLI leaks and the Firefox warning remain open. This follow-up is documentation-only and does not require another build.

## 2026-10-16 — Runtime process-isolation test and task-save warning (CI pending)

To go beyond static unit tests without installing an unresolved browser-flagged build, a new `proxy_env_probe` child executable verifies that **actual spawned QProcess children** receive only the application-managed proxy values when explicitly scoped. A second child representing an external program must not receive those invented values. It checks the running test process environment is unchanged. This executes on Windows and Linux runners using synthetic credentials only. **Not** a full GUI, real-proxy, or Windows Credential Manager integration test.

A new `task_secret_risk` helper warns on selected obvious credentials in extra rclone flags, source/destination URL userinfo, token query fields and Authorization headers before saving `tasks.bin`. Cancel is the default; no secret is displayed; explicit Save remains available. The serialized task file is still **not encrypted** and credential detection is best-effort, not comprehensive. Older tasks are not rewritten by this change. CI pending; no release.
