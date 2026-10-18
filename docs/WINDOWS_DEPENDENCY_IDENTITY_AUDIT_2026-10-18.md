# Windows dependency identity and embedded-version audit (2026-10-18)

Status: **CI-validated 2026-10-18, run #39143956529.** Development branch only. Release remains disabled.

## Evidence

The text-only Windows Authenticode report from the last successful run
[#39137148599](https://github.com/mctransik/RcloneBrowser/actions/runs/39137148599)
records 26 packaged executables/libraries:

- 13 Qt 6.10.3 DLLs/plugins: Valid embedded Authenticode, The QT Company Oy
- 10 VC++ runtime DLLs: Valid embedded Authenticode, Microsoft Windows Software Compatibility Publisher (metadata 14.51.36247.0)
- icuuc.dll: Valid Microsoft **Catalog** signature (not embedded), version metadata 72.1.0.4, runner-context-dependent
- RcloneBrowser.exe: NotSigned, PE file version **4.0.1**, inconsistent with project VERSION 4.0.2
- rclone/rclone.exe: NotSigned, PE version 1.75.2; upstream PGP and pinned archive digest are checked separately by the build

The mistaken Windows resource version is changed from 4.0.1 to 4.0.2
in src/resources.rc. The CI check will reject any future mismatch
with the root VERSION file.

## New fail-closed component gate

After the existing Windows Authenticode audit, the workflow runs
scripts/audit_windows_dependencies.py against both the **actual package
files** and its **Windows Authenticode CSV**. Every EXE/DLL must have a
corresponding record and matching path, SHA-256 and file size.

The policy requires the reviewed class, expected signing publisher
and signature type of Qt DLLs and approved MSVC runtime DLLs. It enforces
Qt version 6.10.3, application version from root VERSION and bundled
rclone PE version 1.75.2. No unknown or unsigned third-party DLL can
pass the current policy. The unusual icuuc.dll catalog-signed case is
handled explicitly rather than described as an embedded signature.
Any changed supplier, status or dependency needs manual review.

On success, the job generates a **text-only JSON component inventory**
outside the distributable ZIP. It records relative paths, SHA-256,
file sizes, component families, Windows PE versions and signature
identities. The separately uploaded artifact is called
windows-dependency-inventory.

The validator's 14 unit tests use **synthetic local files** and passed
before committing. They exercise a good package, modified DLL, unknown
DLL, unsigned Qt, wrong Qt signer/version, wrong app/rclone version,
missing or duplicated CSV records, path traversal, unexpected ICU
signature, unexpected Microsoft signature and missing EXE.
The real 26-row Authenticode CSV was also separately passed through
the component classification policy (13 Qt, 10 Microsoft VC++,
1 ICU, 2 programs), with no binary execution.

## Limits and pending validation

- Windows' Get-AuthenticodeSignature supplies the trusted signature
  assessment. The Python layer checks its CSV; it is not itself a
  cryptographic Authenticode validator. Signer subjects can change
  legitimately; changes intentionally stop the build for review.
- ICU is catalog signed in the GitHub Windows runner; the catalog
  may not exist on a different Windows machine.
- EXE/DLL version resources are not by themselves proof of genuine
  upstream binaries. The rclone archive has separate PGP/hash checks.
- This is a Windows **binary component inventory**, not a complete SPDX
  or CycloneDX SBOM; license obligations, transitive source dependencies,
  and malicious-code detection need further review.
- **Pending:** rerun the Windows/Linux CI on this specific source
  change; confirm the new check actually succeeds and the separate
  inventory artifact contains exactly the expected components.
- The 2026-10-11 Firefox malware warning remains unexplained.
  The release job must remain disabled.

## Confirmed GitHub Actions result — 2026-10-18

[Development workflow run #39143956529](https://github.com/mctransik/RcloneBrowser/actions/runs/39143956529)
completed **successfully** (attempt 1) for commit
\`df22bef3f48c7e6fb946436dbe0421ff5571a4d3\`.

Evidence from the Windows job and the downloaded text-only
\`windows-dependency-inventory\` artifact (ID \`12142103271\`,
containing \`windows-dependencies.json\`):

- \`Windows component identity check passed: 26 EXE/DLL files with expected family, signer and version metadata.\`
- The JSON is schema \`rclonebrowser.windows-dependencies.v1\`,
  reports **26** files and expected versions
  application \`4.0.2\`, Qt \`6.10.3\`, rclone \`1.75.2\`.
- Actual \`RcloneBrowser.exe\` PE version is now **4.0.2**, corrected
  from 4.0.1. Its SHA-256 is
  \`65b1f6e0ca3f4afe2bb8e397c215a218670725d2755a4dc57c33ff101b53277a\`.
- Reported families: **13 Qt**, **10 Microsoft VC++**, **1 catalog-signed
  ICU**, **1 project EXE**, **1 bundled upstream rclone EXE**.
  Windows Authenticode report: **24 Valid, 2 NotSigned,
  0 abnormal statuses**. The two unsigned files are project and rclone;
  upstream rclone is independently PGP+SHA256 verified in packaging.
- New Python unit suite: **14/14 dependency identity tests passed**;
  all Python tests: **39/39** passed on Windows and Linux.
  CTest: **9/9** on each platform.
- Existing Windows package content gate: **69 files** verified.
  ClamAV: **70 current Windows files / 0 reported detections**;
  **544 historical files / 0 reported detections**.
- Verified downstream provenance subject: actual **inner Windows
  application ZIP**, SHA-256
  \`342686604cc20b9c628f6130e92e7b7774c2b917c13c33a16a53c5df52815cec\`.
  The Github attestation was created and independently verified.
- Release job remained **skipped/disabled**, not published.

This only establishes the results reported by this Windows runner and
the tested packaging/attestation policy. It cannot establish malware
freedom, independently audit source-to-binary reproducibility, resolve
license obligations or explain the historical Firefox malware warning.
