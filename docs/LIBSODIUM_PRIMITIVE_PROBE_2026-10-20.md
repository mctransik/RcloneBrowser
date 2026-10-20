# First libsodium integration — isolated primitive probe (2026-10-20)

> **Not production crypto. No changes to task files, saved credentials, or the
> application binary. The existing release gate remains closed.**

## Scope

The new `tests/test_portable_crypto_probe.cpp` runs **only** when the build
option `RB_BUILD_PORTABLE_CRYPTO_PROBE=ON` is selected. The default remains
OFF. No libsodium object code is linked into the `RcloneBrowser` executable.

The Ubuntu CI job installs `libsodium-dev` and `pkg-config` and explicitly
enables the isolated CTest target `portable_crypto_probe`. The Windows
build/test/package steps remain unchanged and do not depend on libsodium.

This first probe demonstrates use of **upstream libsodium C APIs**, not
homegrown cryptographic primitives:

- `crypto_pwhash` with explicit `crypto_pwhash_ALG_ARGON2ID13` and a fresh
  random salt derives a temporary wrapping key from an invented password.
- `randombytes_buf` generates a fresh 32-byte DEK and fresh nonces.
- `crypto_aead_xchacha20poly1305_ietf_encrypt/decrypt` wraps the DEK
  and independently encrypts an invented task payload with two separate
  nonces and domain-separated associated data including a random file ID.
- Synthetic Qt tests cover round-trip, wrong password, payload tampering,
  wrapped-DEK tampering, file-ID and salt alteration, randomized outputs,
  and the authenticated empty payload.
- On all decryption errors, the caller receives **no** plaintext.
  Derived ephemeral keys are wiped with `sodium_memzero`; Qt byte buffers
  remain subject to Qt's implicit-copy and memory-erasure limitations.

## Important safety limitations

1. This test uses `crypto_pwhash_OPSLIMIT_INTERACTIVE` and
   `MEMLIMIT_INTERACTIVE` so development CI is practical. **These are not
   approved production KDF parameters.** Benchmark MODERATE or an independently
   reviewed security target before choosing and freezing the file format.
2. This is a C++ **in-memory test struct**, not a defined `RBTSENC2` envelope,
   file parser, or a migration method. It does not store or authenticate
   portable KDF settings, slot counts, a canonical header or binary layout.
3. CI's Ubuntu system library comes from the runner's apt repositories and
   is **not** pinned to immutable build inputs in this experimental test.
   It must be pinned/verifiably sourced before integrating a release
   dependency. Verify package license notices, provenance and SBOM content.
4. The official libsodium `1.0.22` immutable point release and separately
   signed archives (not rolling `stable` snapshots) are candidates for
   a future Windows dependency review. Do not download arbitrary unsigned
   binaries, disable scanning or put a third-party DLL into releases based
   on this probe.
5. No DPAPI fallback is implemented. A future optional DPAPI slot must
   wrap the **same DEK** and keep the password recovery slot intact.
6. No production task-file encryption, no main-password GUI, no password
   persistence, no backup migration, and no cross-OS encrypted-file recovery
   are implemented. The old `tasks.bin` stays plaintext.

## Next review gates

- [ ] Successful CI including `portable_crypto_probe` on Linux.
- [ ] Confirm the Windows job stays independent and successful.
- [ ] Pin and verify the exact Windows and Linux libsodium release input,
      license, and runtime/static-link architecture.
- [ ] Define and review canonical authenticated `RBTSENC2` header,
      metadata, key slots, KDF limits and compatibility policy.
- [ ] Exercise Windows and Linux with **identical synthetic encrypted files**
      only after format and implementation reviews.
- [ ] Add negative tests for truncation/oversize, KDF exhaustion, swapped
      slots and rollback/migration behavior.
- [ ] Complete remaining release-signature, antivirus and licensing gates.

Upstream references:
https://doc.libsodium.org/usage
https://doc.libsodium.org/password_hashing/default_phf
https://doc.libsodium.org/secret-key_cryptography/aead/chacha20-poly1305/xchacha20-poly1305_construction
https://download.libsodium.org/libsodium/releases/README.html
