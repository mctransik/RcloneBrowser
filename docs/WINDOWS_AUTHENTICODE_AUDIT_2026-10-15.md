# Windows Authenticode and component provenance audit — 2026-10-15

**Status: implemented on development branch, CI execution pending. No malware-free verdict or release approval.**

The Windows `Update` job now runs `scripts/windows_authenticode_audit.ps1` against the **assembled** Windows package, after the ZIP has been made and before an `UNSCANNED` build staging artifact is uploaded. The audit:
- Enumerates all packaged `.exe` and `.dll` binaries (including the main application, bundled rclone, Qt modules and VC++ runtime).
- For each binary, collects a SHA-256, relative path, size, Windows `Get-AuthenticodeSignature` status and signature type, signer certificate subject/issuer/thumbprint/validity dates, timestamp signer subject when exposed, and Windows version metadata (company, product, version and original filename).
- Distinguishes `Valid` from `NotSigned`. The app and bundled official rclone have been observed as unsigned in the earlier static audit; this alone is neither malware evidence nor clearance. The rclone source ZIP is separately pinned and verified against a signed upstream SHA256SUMS list in the existing build.
- **Fails the Windows job on an unexpected signature state** (e.g. a purported signature that Windows judges invalid, untrusted or malformed), allowing detailed manual review. This fail-closed gate is subject to initial Windows-runner validation; trust-store/chain-retrieval failures may also stop the build.
- Uploads only `windows-authenticode-files.csv` and `windows-authenticode-summary.txt` as the separate `windows-authenticode-audit` GitHub artifact, **outside** the distributable Windows ZIP. The new report does not change the hashes of packaged files.
- Does not modify security settings, add signatures, acquire a code-signing certificate, or execute any historical suspect binaries.

**Limitations:** Microsoft's `Get-AuthenticodeSignature` reports Windows' signature assessment for the test runner, not an independent security certification. Catalog signature resolution is possible, and an apparently valid signature does not establish that a file is safe. No independent online certificate revocation check is promised. Version metadata can be forged and must never substitute for a verified signer. Some legitimate open-source libraries are unsigned. Any abnormal status must be analyzed before setting an expected-signature policy.

**Firefox malware warning:** remains an open release blocker. The Firefox-blocked historical ZIP has not been uniquely tied to a specific SHA-256. The earlier `productionresults*.blob.core.windows.net` download host is consistent with GitHub Actions artifact delivery, whereas `release-assets.githubusercontent.com` corresponds to GitHub Releases; neither hosting provider's legitimacy is proof of the payload's harmlessness. No new artifact should be described as malware-free because its signatures and ClamAV scans look normal.

Reference: [Microsoft Get-AuthenticodeSignature](https://learn.microsoft.com/en-us/powershell/module/microsoft.powershell.security/get-authenticodesignature)

**Release stays disabled.** Results must be reviewed before any further policy changes.
