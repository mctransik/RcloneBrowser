# Security review and release gates — 2026-10-12

> **Status: open review, not certification.** This note records observable evidence and specific remaining tasks for the independent cloud-management prototype on `feature/cloud-control-ui-mvp`. No complete source audit, binary malware scan, penetration test, or independent verification of the distributed executables has taken place. **Do not treat this as permission to override security-software warnings or use the prototype with production data.**

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
