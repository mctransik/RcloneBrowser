# Windows dependency identity and embedded-version audit (2026-10-18)

Status: **Locally tested, GitHub Windows CI pending.** Development branch only. Release remains disabled.

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
