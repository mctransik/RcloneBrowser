# Windows build artifacts — static comparison (2026-10-13)

> **Security investigation, NOT a clean bill of health.** Three public GitHub Actions Windows build artifacts were retrieved using GitHub's artifact API and inspected offline, without executing `RcloneBrowser.exe`, `rclone.exe`, or any bundled DLL. The blocked Firefox download has **not** been uniquely mapped to one of these artifacts, and no independent malware scanner or full certificate-chain verification was used.

## Evidence sources

| Workflow date (UTC) | GitHub Actions run | Source revision | Windows artifact ID | Artifact outer ZIP SHA-256 |
| --- | --- | --- | --- | --- |
| 2026-10-10 | [38067162852](https://github.com/mctransik/RcloneBrowser/actions/runs/38067162852) | `185c843b9c6b` | `11674349574` | `3f3698d2efa7a8c155b4ab1a89485347b1660bb6b2a775c23de60d00d8cf78da` |
| 2026-10-11 | [38149605396](https://github.com/mctransik/RcloneBrowser/actions/runs/38149605396) | `11bcb6f51dd8` | `11706221527` | `3b669bfaee67ac76f46b4c3296182d54737a239942d5f4b3e891383ab8ddbbaa` |
| 2026-10-12 | [38235694380](https://github.com/mctransik/RcloneBrowser/actions/runs/38235694380) | `4cd546d625c5` | `11740433824` | `08dba9e2035c37f27fdfe9eecd659b237ab54fb3445a09dd719b59e8483f58dd` |

All three locally calculated outer archive SHA-256 values matched the respective SHA-256 digest reported by GitHub's Actions artifact metadata. **This proves agreement with those repository artifacts, not malware safety.** The user screenshot supplies names/approximate sizes but not the ZIP digest or artifact ID, so no particular row is confirmed to be the blocked download.

## Full-file comparison

Each Actions artifact is an outer ZIP containing **67 entries**. The complete contents of these three ZIPs were compared by SHA-256 after reading each ZIP member in memory.

**Only two files differ between the three archives:**

- `RcloneBrowser.exe` (changes with development revisions).
- `docs/README.md` (changes as documentation evolves).

**All other 65 members are byte-for-byte identical** across these three artifacts. This includes the bundled `rclone/rclone.exe` and **24 DLLs** (Qt, Qt plugins, Visual C++ runtime, and other dependencies), plus non-executable assets and settings.

| Package member | Oct 10 SHA-256 | Oct 11 SHA-256 | Oct 12 SHA-256 |
| --- | --- | --- | --- |
| `RcloneBrowser.exe` | `fd5d46c8b364c3c30004ed33047c4bb9bc77a0e2bbc764d3f905d79780e594d3` | `f1c365d7ee42eaf21c696343db6908b62a0039474e1a931b0b3b23c49a121ef7` | `78293db5b19e4988b16d0ef687cdc77a3ea9a11ba1d2214af87a92b0f8b5e7ca` |
| `rclone/rclone.exe` | `41eab3e118d264920b62c3327966c5ece0dc4dd715ee617d9e30450d8ead95a4` | same | same |
| `Qt6Core.dll` | `b712b4754588e89f855dc0aa087d6304b2fd21b0e0639d1cdeb9cb969bd5fb72` | same | same |

Because the GitHub ZIP wrapper itself is also different per build, **the shared download/asset name must not be used as an identity or integrity check**.

## Static Windows PE observations

The Windows executable images and DLLs were read as bytes (no process execution). The PE header/security-directory inspection showed:

- `RcloneBrowser.exe` from all three builds: **no embedded Authenticode certificate table** (`WIN_CERTIFICATE` size zero); Windows x64 GUI PE with ASLR, DEP and CFG flags enabled.
- The bundled `rclone/rclone.exe`: **no embedded Authenticode certificate table**. rclone can nevertheless be authenticated via its independent upstream PGP-signed archive verification in the build process; **upstream PGP verification and Authenticode signing are different security properties**.
- Several Qt and Microsoft DLLs contain a Windows certificate table. **The presence of that table is not proof that the signer identity, revocation status, timestamp, chain of trust, or content signature was successfully validated.** A few bundled DLLs lack a certificate table, including `icuuc.dll`.
- The app's imported DLL names in the reviewed executable are conventional for this Qt/Windows build (Qt6Core, Qt6Gui, Qt6Network, Qt6Svg, Qt6Widgets, Windows API libraries, MSVC runtime). This superficial observation does **not** rule out harmful behavior in code, runtime-loaded libraries or subprocesses.
- The ZIP central directories had **no duplicate entries, path traversal, absolute-path entries, symlinks or encrypted members**. These checks cover container structure, not executable trust.

## Interpretation of the Firefox warning

The earlier browser screenshots showed explicit **"Diese Datei enthält einen Virus oder Malware"** detections on `productionresults…blob.core.windows.net` download URLs for the Windows ZIP, without a detailed malware family label. GitHub documents `*.blob.core.windows.net` as part of Actions artifact storage, while `release-assets.githubusercontent.com` is for releases: https://docs.github.com/en/actions/reference/runners/github-hosted-runners .

**Not established:** the offending file hash, detection engine/signature, whether Firefox matched the ZIP bytes versus another property of the request, or whether the same bytes were tested via other distribution URLs. The earlier downloaded release packages and the Actions artifacts are not necessarily equal. Do not classify the warning as a false positive from these observations alone.

**Important limitation:** A successful CI build, ZIP structure inspection, conventional import table, app hardening flags, or presence of DLL certificate tables do **not** constitute an antivirus verdict.

## Follow-up gates

1. Correlate the actual blocked browser download with its **exact artifact ID and full SHA-256**, without bypassing Firefox or allowing blocked content.
2. Independently validate Authenticode signatures **where present** on bundled DLLs and verify the upstream signature/checksum evidence for the exact bundled rclone release.
3. Use a controlled, reputable independent malware-analysis process for the exact flagged sample; agree on privacy/exposure implications before uploading any binaries to third-party scanning services. Review threat detections rather than assuming false positives.
4. Pin and record release dependencies, including rclone, plus hashes and source-to-binary provenance; GitHub Actions currently downloads the then-current upstream version at build time.
5. Add package-manifest and dependency-inventory checks in CI, then complete credential, command-argument, filesystem and network-threat reviews.
6. Block production release, merge to `main`, and runtime testing of browser-blocked builds until the investigation and release review justify them.

**Result:** Stronger package-level traceability and a narrowed binary difference; **malware status remains unresolved**.


## 2026-10-15 — Native Windows Authenticode audit pending CI

The Windows development job now analyzes the assembled `.exe` and `.dll` payload using native `Get-AuthenticodeSignature` and records file hashes, signer details and version metadata in a separate text-only GitHub artifact. Unexpected signature states fail the Windows job. **This is only a newly implemented check, not yet a validated result**; no Windows publisher certificate has been issued for this project. See [Windows signature audit](WINDOWS_AUTHENTICODE_AUDIT_2026-10-15.md). The historical Firefox detection remains unresolved.
