# Windows Authenticode and component provenance audit — 2026-10-15

**Status: completed and verified on Windows GitHub Actions 2026-10-15. No malware-free verdict or release approval.**

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

## Observed Windows GitHub Actions results (2026-10-15)

Evidence: [run #38713775680](https://github.com/mctransik/RcloneBrowser/actions/runs/38713775680), commit `7604b18b4aa442acfd1968de588b1ed3a9f4fe24`. The text-only `windows-authenticode-audit` artifact ID `11961727207` was downloaded, opened and independently summarized. The actual runner completed the new audit step successfully.

| Payload class | Number | Windows signature status / verified signer |
|---|---:|---|
| Qt libraries (Qt6 and plugins) | 13 | `Valid`, embedded Authenticode; **The QT Company Oy**, DigiCert-issued code-signing certificate |
| Microsoft Visual C++ DLLs | 10 | `Valid`, embedded Authenticode; **Microsoft Windows Software Compatibility Publisher**, Microsoft-issued chain |
| `icuuc.dll` | 1 | `Valid`, **Catalog** signature from **Microsoft Windows** in the *GitHub-hosted Windows runner* environment; no embedded Authenticode signature implied |
| `RcloneBrowser.exe` | 1 | `NotSigned` |
| `rclone/rclone.exe` | 1 | `NotSigned`, independently upstream-PGP+SHA256-verified during packaging |
| **Total** | **26** | **24 `Valid`, 2 `NotSigned`, 0 unexpected or invalid signature statuses** |

All 13 Qt DLLs showed the same Qt signer certificate thumbprint (`3D89F77A33C5AD89F45F681848958E755B727C5A`). The 10 Microsoft C++ runtime DLLs showed the Microsoft publisher thumbprint (`2650247AC56048BC7928663C02733D25898A1D6E`). The Windows runner validated `icuuc.dll` through a machine catalog; that outcome may not reproduce on a separate computer without the identical trusted catalog. The catalog result should not be misreported as a directly embedded signature.

Additional evidence from the *same* run: Windows CTest 3/3 passed; pinned rclone upstream PGP signature checked; current Windows ClamAV scan: 70 files, 0 detections; separate historical ClamAV investigation: six pinned outer ZIPs verified, 408 files and 0 detections. The public release job was skipped/disabled.

**Important caveats:** Windows `Valid` identifies an acceptable signature in that runner environment. It does not demonstrate that the source code is benign, guarantee revocation validity on other machines, eliminate Firefox's blocking reputation, or replace app-level code signing. Both `RcloneBrowser.exe` and the bundled `rclone.exe` are unsigned as Windows PE files. The **exact Firefox-blocked ZIP still has not been identified**, so a false-positive conclusion is not justified.

Minor report-cleanup committed afterward: the report formerly contained an initial `result=not_completed` *and* a later `result=audited_signatures_only`, which could be misleading even though the job succeeded. The script now uses `audit_started=true` as a nonterminal marker and writes only the applicable terminal `result=` line. This cleanup has not itself been rerun in GitHub Actions.
