# libsodium 1.0.22 Windows detached-GPG verification (2026-10-21)

> **Experimental CI dependency only.** No persistent encryption, no task
> migration, no libsodium DLL in released application archives. This document
> describes draft PR #13 stacked on #12, #11 and #10. Do not merge/release
> based solely on these tests.

## Trust material

- Upstream immutable artifact:
  `https://download.libsodium.org/libsodium/releases/libsodium-1.0.22-msvc.zip`
- Upstream detached OpenPGP signature: same URL plus `.sig`
- Artifact SHA-256:
  `3e03a726fac4bc09cb61d8f29d658ef7a5eca0811de59082130414f7ca2e4279`
- Detached signature SHA-256:
  `ec59666f62b853576105b5e151a21672c95a057aced84ef1e1de8fc850704eb4`
- Both hashes independently listed by the libsodium maintainer on the
  **GitHub release 1.0.22-RELEASE**.
- A **public** GPG signing key is checked in at
  `security/libsodium-release-signing-key.asc`. Its armored bytes were
  copied from the canonical libsodium installation documentation maintained
  by `jedisct1`: 
  `https://github.com/jedisct1/libsodium-doc/blob/master/installation/README.md`.
  It is a public verification key, never a private key.

## CI enforcement on Windows

1. Download and hash-check the point-release ZIP.
2. Download and hash-check the matching detached signature.
3. Locate GnuPG or **fail closed** (never skip signature verification).
4. Create an isolated empty GnuPG home under `RUNNER_TEMP` and import only
   our reviewed official verification public key.
5. Require exactly one primary public key in the isolated keyring; inspect
   its fingerprint.
6. Verify the exact detached signature against the exact downloaded
   archive; require successful GPG exit status **and** `VALIDSIG` machine
   status before any archive extraction.
7. Only then extract headers, the import library and the DLL for the
   **opt-in test executable**. Never put those test dependencies in the
   Windows release ZIP.

A cryptographically valid detached signature is stronger provenance evidence
than a checksum served beside a file. However, **its identity is only as
trustworthy as the reviewed, independently sourced public key**. Review key
changes in pull requests; never fetch a new verification key automatically
from the same download endpoint at build time. If upstream rotates keys,
explicitly review and approve the new trust anchor.

## Remaining work

- [ ] Verify actual Windows CI signature-check execution and successful build.
- [ ] Record/pin the actual GPG signing fingerprint from CI validation output.
- [ ] Add adversarial test cases (wrong signature, modified ZIP, missing key);
      confirm fail-closed behavior beyond a happy path.
- [ ] Review libsodium upstream release + licensing notices, binary metadata,
      Authenticode status and SBOM inclusion before *any* production linking.
- [ ] Agree on a canonical portable, authenticated v2 file layout and
      cross-platform test vectors. No production encryption enabled.
- [ ] Address outstanding Firefox detection, Windows code-signing and other
      release gates.

Reference:
- https://github.com/jedisct1/libsodium/releases/tag/1.0.22-RELEASE
- https://download.libsodium.org/libsodium/releases/README.html
- https://doc.libsodium.org/installation
