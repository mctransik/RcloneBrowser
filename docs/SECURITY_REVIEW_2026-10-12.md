# Security review and release gates — 2026-10-12

> **Status: open review, not certification.** This note records observable evidence and specific remaining tasks for the independent cloud-management prototype on `feature/cloud-control-ui-mvp`. No complete source audit, binary malware scan, penetration test, or independent verification of the distributed executables has taken place. **Do not treat this as permission to override security-software warnings or use the prototype with production data.**

**Static package inspection update (2026-10-13):** See [Windows artifact comparison](WINDOWS_ARTIFACT_STATIC_AUDIT_2026-10-13.md). Three public Actions archives (10–12 October) were examined without execution. All contained 67 entries; only the app executable and README differed, while the bundled rclone and all 24 DLLs were byte-identical. The user’s exact blocked download is still unidentified and its malware classification unresolved. This is not a scanner verdict.

## Reviewed scope and evidence

- Development revision reviewed: `4cd546d625c5f1a2ece3168c43f0ee4f34908262`.
- GitHub Actions run: https://github.com/mctransik/RcloneBrowser/actions/runs/38235694380 — completed successfully on Windows and Linux, release job **skipped**.
- The existing CTest suite passed **1/1** on both platforms. CI is not currently a complete application-security test suite or a substitute for manual GUI checks.
- The separate PGP verification unit suite passed **5/5** on both platforms: correct signature+digest, altered archive, altered signed text, wrong signer, and missing archive checksum entry.
- Both jobs logged successful verification of the downloaded rclone archives against a PGP-signed checksum list and the pinned signing fingerprint `FBF737ECE9F8AB18604BD2AC93935E02FF3B54FA`.
- The workflow produced Windows and Linux artifacts for that development revision. The GitHub artifact SHA-256 digest identifies each **artifact ZIP wrapper**, not an independent clean-malware verdict for every executable inside.
- The rclone updater source uses rclone's `selfupdate` with an output staging location, checks the staged executable's reported version, and only then replaces the managed rclone copy. This is a **code-path observation**, not a fully verified runtime security claim.
- Inherited application code is still in use, and other paths (file access, remote configuration, mounts, stream playback, logging, and third-party dependencies) have not yet received a complete adversarial review.

## Blocker 1 — Firefox malware download warning

An earlier development ZIP reportedly triggered the Firefox message **"Diese Datei enthält einen Virus oder Malware"** while a Bitdefender check was not reported to flag it. **The precise flagged file, version, download URL, digest, detection record and root cause have not yet been established.** Do not infer that the alert was a false positive merely because the source is public, CI passed or another scanner remained silent.

Mozilla documents different categories for malware findings and uncommon software; the explicit malware wording warrants investigation:
- https://support.mozilla.org/en-US/kb/where-find-and-manage-downloaded-files-firefox
- https://support.mozilla.org/en-US/kb/how-does-phishing-and-malware-protection-work

**Evidence to capture without bypassing protections:** screenshot with the full warning text; affected archive filename; original download page / URL; approximate date; any threat or detection identifier shown by the browser/security product; and, if the original archive is safely available without restoring blocked content, its locally measured SHA-256 hash. Redact usernames and any local folders or remote names.

**Investigation:** compare the flagged artifact with its precise GitHub workflow/run and associated source commit; independently examine the package contents and signing status in a controlled analysis environment; evaluate external reputation/detection evidence; identify whether the classification refers to the executable, packaged dependencies or distribution URL. A clean scan by one product does not override a detection by another.

**Gate:** No recommendation to bypass Firefox, Windows SmartScreen, antivirus products or quarantine; no production-ready release while this is unresolved.

### Additional screenshot evidence — received 2026-10-12

The user shared a Firefox download-history screenshot (kept in the conversation; **not uploaded to this public repository**). It shows multiple downloads named `RcloneBrowser-4.0.2-windows-x64.zip`:

- Two entries labeled **"Diese Datei enthält einen Virus oder Malware"** under **"Gestern"** (relative to 2026-10-12), with an origin shown as `productionresults…blob.core.windows.net`.
- Additional entries from similar `productionresults…blob.core.windows.net` hosts at **0 bytes**. The screenshot alone does **not** identify why those attempts resulted in 0 bytes.
- An older entry around **43.1 MB** from that storage domain and two entries around **43.2 MB** from `release-assets.githubusercontent.com`, without that explicit warning displayed next to them.
- GitHub's own runner network documentation lists `*.blob.core.windows.net` for Actions workflow artifact transfer, and `release-assets.githubusercontent.com` for published release assets: https://docs.github.com/en/actions/reference/runners/github-hosted-runners .
- Mozilla's documentation explicitly distinguishes the **virus/malware** download warning from the **uncommon download** warning: https://support.mozilla.org/en-US/kb/where-find-and-manage-downloaded-files-firefox .

GitHub Actions workflow run artifacts created on 2026-10-11, including run IDs `38149605396` and `38145485000`, each include a Windows artifact named `RcloneBrowser-4.0.2-windows-x64` with GitHub-reported size **approximately 43.08 MiB**. Other successful runs on the same day produced similarly named/sized artifacts. **This is a temporal and size correlation only; the screenshot does not uniquely link either blocked download to a particular run ID, commit SHA or archive digest.**

**Current interpretation:** The suspicious downloads appear consistent with GitHub-hosted Actions artifacts rather than a random third-party site. However, the origin's being a legitimate GitHub storage endpoint does **not** establish that its payload was benign. Likewise, a successfully downloaded release asset is not automatically safe, and different builds with the same filename must not be assumed byte-for-byte identical.

**Firefox details dialog — screenshot supplied 2026-10-12:** The user opened the download details. The dialog explicitly repeats **"Diese Datei enthält einen Virus oder Malware"**, adding only **"Diese Datei enthält einen Virus oder Malware, welche Ihren Computer beschädigen wird."** It presents **"Datei löschen"** and **"Download erlauben"**. **No malware family name, engine, signature, detection code, checksum, artifact ID or GitHub run ID is shown**. The warning therefore confirms that Firefox classifies the download as malware, but the details panel does **not** establish which payload component or detection rule caused it. Do not click **"Download erlauben"** or disable protection to pursue this investigation. Mozilla's documentation explains that Firefox's download protection uses the Google Safe Browsing service for potentially malicious downloads: https://support.mozilla.org/de/kb/suchen-und-verwalten-heruntergeladener-dateien?redirectlocale=de&redirectslug=Suchen-und-Verwalten-von-heruntergeladene-Dateien .

**Next evidence needed:** If available without restoring or bypassing blocked content, capture the corresponding GitHub Actions run URL or artifact ID from browser/history context, any specific scanner detection name or ID, and the SHA-256 of an *already safely available, non-quarantined* archive. Do **not** make copies of credentials, temporary download URLs carrying query tokens or private paths public.

## Blocker 2 — Build inputs and source-to-binary traceability

The workflow at [`.github/workflows/update.yml`](../.github/workflows/update.yml) downloads the **then-current** rclone release from `downloads.rclone.org/version.txt` independently in the Windows and Linux jobs, albeit with upstream PGP signature verification. Consequently two builds of the same source commit at different times can include different versions of rclone. That is not a signature bypass, but makes artifact comparison and reconstruction harder.

**Required before release:**
1. Resolve the desired **specific rclone version** as an approved build input, consistent across Windows and Linux.
2. Preserve the authenticated per-platform checksum and upstream signer evidence in the release build record.
3. Inventory Qt, compiler/runtime, operating-system image, packaging tools, executable metadata and any downloaded build dependencies. Assess reproducibility and dependency trust, not merely whether a pinned tool URL was used.
4. Generate a machine-readable component inventory / SBOM and third-party notices for the *actual* distributed package.
5. Link exact source revision, CI run, release artifact hashes, build provenance and any later signing evidence. Do not describe hashes, attestations or signatures as proof that an application contains no malware.

## Blocker 3 — Runtime trust and credential handling

These are **review targets**, not confirmed vulnerabilities:
- `src/rclone_updater.cpp` can fall back to the configured external rclone executable when the managed one is unavailable. Since rclone's own `selfupdate` performs the signature checks, the trustworthiness of that initial verifier matters; document and test the bootstrap/trust assumptions.
- `src/utils.cpp` passes the password for encrypted rclone configuration to child processes via `RCLONE_CONFIG_PASS`. Review inherited process environments, UI lifetime and error-output handling, and ensure logs never disclose passwords, provider tokens or secrets.
- `src/dual_pane_widget.cpp` uses copy confirmation and rclone `--ignore-existing`; review same-source/cross-remote destination handling, local symlinks/junctions, recursive copy prevention, backend behavior, and races. Test with **disposable data**. An option intended to avoid overwriting is not proof against every filesystem or remote-side race.
- Review the inherited upload/delete/move/sync/mount/stream flows and transfer logs. New controls must not imply that the rest of the inherited UI cannot alter or delete remote files.
- Confirm the app asks before destructive actions and does not silently elevate privileges or transmit diagnostic data to a project-hosted analytics service.

## Release acceptance checklist

- [ ] Investigate and document the exact Firefox warning and its disposition.
- [ ] Complete independent binary/package analysis appropriate for a test release, including bundled dependencies.
- [ ] Pin/record approved build inputs and retain full source-to-artifact traceability.
- [ ] Conduct focused credential, filesystem, network and rclone command-argument security review.
- [ ] Add automated regression tests for unsafe copy targets, updater refusal paths and affected threat scenarios.
- [ ] Complete [third-party notices](THIRD_PARTY_REVIEW.md) and [privacy review](PRIVACY.md).
- [ ] Finish [code-signing and publisher policy](CODE_SIGNING_POLICY.md); current project executables remain **unsigned**.
- [ ] Obtain explicit release approval. Do not silently merge the prototype into `main`, rewrite the historical RC1 tag, or publish a new release.

## Outcome

**CI build and existing automated tests: passed.**
**Application-wide security / malware status: unverified.**
**Firefox malware-warning investigation: open.**
**Public production release authorization: not granted.**

This file documents review findings and open gates only. It does not change executable behavior.

## Build pinning implemented on development branch — 2026-10-13

**Code committed, CI validation pending.** The development workflow now fixes bundled rclone to **v1.75.2** for Windows and Linux. The selected official upstream release was published on 2026-10-09 as a security update. The exact archive ZIP SHA-256 values independently match the upstream GitHub release assets and the successful 2026-10-12 build logs:

- Windows `rclone-v1.75.2-windows-amd64.zip`: `8a9db2bad1dc79a65b3348fd857069598e2aab7cf4d118b77b1df23055aff818`
- Linux `rclone-v1.75.2-linux-amd64.zip`: `349ac8fba6ff65d6247043f1750cdcb518ec5d500ef91463a10d37c0ccdf3702`

The workflow **retains the existing PGP signer + signed SHA256SUMS verification** and additionally refuses archives that differ from the approved platform hash. Changes to rclone require deliberate version and checksum review, rather than dynamically using `version.txt` at build time.

The Windows ZIP now aims to include `docs/BUILD_PROVENANCE.txt` (source checkout commit, GitHub run ID/attempt, exact rclone archive/version/hash and Qt version) and `docs/PACKAGE_SHA256SUMS.txt` (per-file payload hashes; the manifest itself is excluded). The Linux AppImage aims to include `usr/share/rclone-browser/BUILD_PROVENANCE.txt`. These are self-reported **traceability records, not cryptographic attestations or malware clearance**. Cross-platform CI still needs to run successfully on this new revision; the actual resulting archives must be inspected to confirm both records are included and correct.

The development-branch `release` job has been **disabled unconditionally** while the Firefox malware investigation is open. This protects only workflow versions containing the change: `main`, historical tags, and prior releases are **not altered**. Do not create new release tags while the old main-branch workflow still permits automatic publication. Before enabling any release path, complete the security review, require explicit human approval, and audit the release workflow and repository rules.

**Still outstanding:** Exact Firefox-blocked sample identification, independent malware verdict, third-party SBOM/license review, other pinned build inputs, runtime credential and filesystem tests, approved code signing, and human release approval.

## Credential handling follow-up — 2026-10-13

Source review on `feature/cloud-control-ui-mvp` identified two low-risk fixes in this development revision:

- `src/main_window.cpp` now recognizes the `RCLONE_CONFIG_PASS` / obsolete `listremotes` hints even at byte offset zero in stderr; the previous `indexOf(...) > 0` skipped that position. This is a **password-prompt reliability fix**, not encryption hardening.
- `src/job_widget.cpp` now requires **explicit confirmation before copying the unredacted rclone command to the operating-system clipboard**. There is no claim of automatic redaction: users who confirm will still copy any included secrets.

These changes require a fresh Windows/Linux CI run and a manual GUI check with disposable credentials. They do **not** eliminate the following open credential-security review items:

1. `src/utils.cpp`: the encrypted-config password is held in process-wide `static QString gRclonePassword` for the application's lifetime and is passed to rclone children through `RCLONE_CONFIG_PASS`. This is a supported rclone mechanism but must be reviewed for lifetime, child process scope, and memory handling. No app-specific disk persistence of that password was observed in this pass.
2. `src/main.cpp`, `src/main_window.cpp`, `src/preferences_dialog.cpp` and `src/rclone_updater.cpp`: the application can **persist proxy URLs containing usernames/passwords in QSettings** and expose them as environment variables for child processes. In portable mode, settings can live in an app-directory INI file. This needs a secure migration/storage design that does not silently delete working proxy credentials.
3. `src/job_options.cpp` + `src/list_of_job_options.cpp`: arbitrary extra rclone flags are saved in the unencrypted task-data file; they may contain secrets if a user enters credential-bearing arguments. Review filename, permissions, and upgrade/migration strategy.
4. `src/job_widget.cpp`, `src/progress_dialog.cpp`, `src/stream_widget.cpp`, `src/mount_widget.cpp`: subprocess output may contain URLs, tokens or sensitive names and currently reaches UI logs unsanitized. Implement a tested, conservative secret-handling policy before allowing logs to be exported/shared; never market simple regex replacements as total protection.
5. Inspect all temporary shell scripts, external player commands, config-file permissions, and production cloud-service behavior. Avoid testing with live accounts or backups until independent release review.

**Status:** Partial source audit only; no runtime inspection or penetration test. Do not claim that passwords or tokens cannot leak. Firefox malware detection and production release gate remain unresolved.


## 2026-10-14 — Windows malware gate added; validation pending

A new mandatory Microsoft Defender scan gate for the Windows package and ZIP is in the development workflow before artifact uploads. Details: [Malware scan gate](MALWARE_SCAN_GATE_2026-10-14.md). **No new scan result is claimed until CI executes this code.** The old Firefox malware warning remains unresolved. Releases are still disabled.


## 2026-10-14 — Defender unavailable, Linux ClamAV alternative pending CI

[Workflow #38582287046](https://github.com/mctransik/RcloneBrowser/actions/runs/38582287046) failed its first Microsoft Defender scan solely because the Windows runner reported Defender was not active, following a failed signature-update attempt. Windows C++ tests/build and Linux passed; **no Windows package was promoted**. The workflow now labels staged Windows inputs explicitly UNSCANNED and scans them using ClamAV with refreshed official signatures on Ubuntu before creating final Windows artifact names. See [new scan policy](MALWARE_SCAN_GATE_CLAMAV_2026-10-14.md). This code is **not yet CI validated**. Neither ClamAV nor Defender can by themselves resolve the Firefox warning.

## 2026-10-14 — Historical artifact scanning verified

[Workflow #38595828400](https://github.com/mctransik/RcloneBrowser/actions/runs/38595828400) completed successfully. The six pinned GitHub Actions Windows ZIPs from 10–11 October all matched their GitHub-reported SHA-256 values; each contained 67 ZIP entries. ClamAV 1.5.4, with refreshed databases (3,628,169 signatures reported), scanned 408 files (404.18 MiB) and reported **0 infected files**. See [historical evidence](HISTORICAL_WINDOWS_MALWARE_AUDIT_2026-10-14.md). The current Windows package scan likewise reported 0 infected files. The scanned Windows builds passed CI; releases remained disabled.

**Do not label the Firefox warning a false positive.** The blocked download has not been uniquely tied to a verified artifact digest. ClamAV detection and browser download-reputation checks are distinct. The warning, independent second-opinion analysis, and release-signing review remain open.

## 2026-10-15 — Atomic saved task storage CI-validated

[Development workflow #38755582451](https://github.com/mctransik/RcloneBrowser/actions/runs/38755582451) passed Windows and Linux builds and CTest (4/4 per platform), including the new `task_storage` tests. The revised saved-task persistence uses `QSaveFile` for atomic commit with direct-write fallback disabled, retains the existing QDataStream format, and restricts Unix task-file permissions on save. See [task storage security evidence](TASK_STORAGE_SECURITY_2026-10-15.md). **No claim is made of encryption of saved rclone flags, Windows ACL hardening, or complete resistance to power-loss/filesystem faults.** Next: propagate save errors to the UI and review confidentiality of task options/logging. Firefox malware-warning investigation remains open and automatic release remains disabled.

## 2026-10-15 — Task save/delete UI error handling CI-validated

[Development run #38766022416](https://github.com/mctransik/RcloneBrowser/actions/runs/38766022416) passed **5/5 CTest suites on Windows and Linux**, including seven new task list transaction tests and the prior atomic file-write suite. The save/edit/delete operations now report failed disk commits to the GUI; in-memory task list mutations are rolled back when persistence fails, and unsuccessful edited fields are restored. See [verified task storage report](TASK_STORAGE_SECURITY_2026-10-15.md). No manual GUI fault-injection test, encryption or complete credential/log handling review is claimed. Windows archive provenance verification passed; automatic release remains disabled and the historical Firefox warning remains unresolved.
