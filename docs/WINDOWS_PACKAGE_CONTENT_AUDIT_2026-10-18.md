# Windows package-content and accidental-secret safeguard (2026-10-18)

> Status: **CI-validated on 2026-10-18**, run #39137148599. This gate does **not** claim to detect all secret types or malware; releases remain disabled.

## Rationale

A historical GitHub Actions Windows artifact was uploaded by the user to VirusTotal
for a second-opinion malware check. Its SHA-256 matched the GitHub-hosted build
exactly. The package came from CI and did not incorporate the user's local
`rclone.conf` or files. A separate preventive gate reduces the risk of
**future packages** accidentally including additional files or plaintext
credentials.

## Implemented gate

The Windows assembly job runs `scripts/audit_release_payload.py` **after**
the file-hash manifest is written but **before** the smoke test and any
Actions artifact upload. A nonzero exit terminates the packaging job:

- Fail on unapproved paths, directories, extra executables and linked file
  or directory entries, including `rclone.conf`, `.env`, keyfiles and
  unplanned text files. Approve only the known portable settings, root DLLs,
  bundled rclone, Qt plugin DLLs, remote-icon PNGs and project docs.
- Verify the expected exact content of generated portable
  `RcloneBrowser.ini` and `qt.conf` so credential-bearing settings
  cannot silently replace them.
- Scan included text documentation/settings for *high-confidence* markers
  of private keys and common credential formats without echoing secret
  values into CI logs.
- Recompute SHA-256 for each member other than
  `docs/PACKAGE_SHA256SUMS.txt`; require exactly one correct manifest
  line for every member.
- Reject missing required payload files, and fail on malformed or
  unreadable content.

`scripts/tests/test_audit_release_payload.py` uses **synthetic local
fixtures**, not real secrets or user data. It covers valid shape,
unexpected rclone.conf, unexpected executable, keyfile, modified INI,
private-key marker, missing required file, altered payload, malformed
manifest, unexpected hidden directory and linked file.

## Deliberate limitations

- This is an **allowlist for package layout**, not a full review of DLL
  origins or the application source. A malicious extra DLL with a
  permitted location is not necessarily caught; separate Authenticode,
  ClamAV, provenance and dependency reviews remain necessary.
- Secret-marker checks are heuristic and apply to **plain text** only;
  they cannot prove no credential is embedded in EXE/DLL binaries,
  compressed formats or PDF bytes.
- The manifest comes from the same build process; matching hashes
  prove internal consistency, not independent publisher authenticity.
- The gate and 11 synthetic regression tests **passed in GitHub Actions**
  on the development branch, run #39137148599, for commit `d870f45`.
  Future changes to packaging or allowlisted files require fresh CI verification.
- The historical Firefox malware warning is still unresolved; automatic
  release remains explicitly blocked.

## Confirmed development CI result — 2026-10-18

GitHub Actions [run #39137148599](https://github.com/mctransik/RcloneBrowser/actions/runs/39137148599)
for source commit `d870f45e4e56cdb6421adf0ac31d6f5ce7bc9dac`
completed **successfully**, first attempt:

- Windows packaging log: `Windows package inventory passed: 69 expected-shape files; no high-confidence text credential markers.`
  The check ran **before** the application ZIP was built and before
  Windows artifact upload.
- Windows Python unit tests: **25/25 passed**, including **11/11**
  `ReleasePayloadGuardTests`. Same Python set passed on Linux.
- CTest: **9/9 Windows** and **9/9 Linux** suites passed.
- ClamAV: **70 files / 0 detections** on current Windows build;
  **544 files / 0 detections** on the eight historic Windows archives.
- Attested actual **inner Windows application ZIP** SHA-256:
  `0ff03673e57b6804e63024cf80f6df181b7bb566285d6d08c4004fc6a0ec2724`.
  Downstream provenance job rehashed it against scan-time checksum
  and independently verified GitHub's SLSA attestation.
- Windows, Linux, historical scan, Windows malware scan and development
  provenance jobs all succeeded; **Release job was skipped by policy**.
- These are the log-observed results for this specific run, not a
  malware-free certification. Firefox's 2026-10-11 malware
  warning remains unexplained, and no browser protection bypass or
  end-user execution is authorized.
