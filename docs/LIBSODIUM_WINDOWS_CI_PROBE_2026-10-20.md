# Windows libsodium CI probe — pinned 1.0.22 (2026-10-20)

**Research build only; not released, not used to encrypt `tasks.bin`.**

## What changed

The independent `test_portable_crypto_probe` suite from draft PR #11 now
has an optional Windows/MSVC build path. The Windows CI job:

1. Fetches the immutable **libsodium 1.0.22 MSVC ZIP**, never a floating
   `stable` package.
2. Checks SHA-256 **before extraction** against
   `3e03a726fac4bc09cb61d8f29d658ef7a5eca0811de59082130414f7ca2e4279`.
   This digest is independently listed on the maintainer's public GitHub
   release page, while the ZIP is downloaded from the official download host.
3. Checks the expected Visual Studio v143 x64 Release import-library, DLL,
   and header locations. The CI step exports a test-only root path.
4. Opts in to `RB_BUILD_PORTABLE_CRYPTO_PROBE=ON` to build the **separate**
   Qt/CTest fixture using that DLL/import library. The DLL is copied only
   to the unit-test executable directory, **never into the app folder/ZIP**.
5. Keeps the existing Linux probe path through `libsodium-dev` and
   `pkg-config`, so no application target links against libsodium.

The primary application target is unchanged. Disabling the option or building
without an explicit Windows test root leaves the ordinary build independent
of libsodium.

## Supply-chain limitations — DO NOT SHIP

- **SHA-256 match proves only archive identity to the recorded hash.**
  The independent detached **GPG/Minisign signature is not yet checked in
  CI**, so this is insufficient for a released cryptographic dependency.
  For production, verify the immutable point release's detached signature
  with an independently pinned, trusted upstream key *and* retain the digest
  check. Official upstream documents its Minisign public key:
  `RWQf6LRCGA9i53mlYecO4IzT51TGPpvWucNSCh1CBM0QTaLn73Y7GFO3`.
- This test uses a development dependency; the later Windows application
  packaging must decide static versus shared linking, version inventory,
  dependency notices, signing checks, EDR false-positive triage, DLL
  integrity checks and SBOM entries. No such runtime integration is
  authorized by this PR.
- Linux currently exercises Ubuntu-provided libsodium 1.0.18; Windows
  exercises pinned 1.0.22. They share the API and are not yet proof of a
  portable cross-OS **file format**. Pinned inputs on both systems and
  cross-platform byte fixtures are future requirements.
- The existing `test_portable_crypto_probe.cpp` uses **in-memory
  synthetic samples** and development KDF settings only; it is not a
  reviewed encrypted file format or credential manager.
- Passing CTest, ClamAV, or package inventory checks does not establish
  malware-free software or a production-grade cryptosystem. Historical
  Firefox warning, third-party licensing and signing remain open.

## Verification checklist

- [ ] GitHub-hosted Windows job: DLL/linker/header validation succeeds,
      `portable_crypto_probe` test runs and succeeds in CTest.
- [ ] Linux CTest still succeeds with independent package installation.
- [ ] The final Windows package audit confirms **no libsodium DLL or
      experimental test executable is in `dist/`**.
- [ ] Detached signature verification, immutable supplier metadata and
      third-party notices are implemented before any app integration.

Sources:
- https://github.com/jedisct1/libsodium/releases/tag/1.0.22-RELEASE
- https://download.libsodium.org/libsodium/releases/README.html
- https://doc.libsodium.org/doc/installation
