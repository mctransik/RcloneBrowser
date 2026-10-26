# Independent portable recovery decoder — research (2026-10-26)

**TEST-ONLY, DRAFT.** This work does not change RcloneBrowser, `tasks.bin`,
the real password handling system, a release, or any on-disk production
storage format. The test fixture and password are intentionally public.

## What is meaningfully independent

Our original `tests/test_portable_v2_envelope.cpp` creates and opens
synthetic `RBTSENC2` encrypted bytes using the same code. It also
reopens a committed encrypted vector that was generated separately.

The new **separate executable**
`tests/test_portable_independent_recovery.cpp` implements a second,
narrow **read-only** decoder. It does **not** call the first test file's
`seal()`, `open()`, simulated memory-lock policy, Qt credential storage,
or any Windows account/DPAPI facility. It uses libsodium's separately
implemented, well-known Argon2id13 and XChaCha20-Poly1305 operations,
not a home-made cryptographic primitive.

The fixture is the **unchanged independent committed 202-byte vector**,
with pinned SHA-256
`6a166dd34aeb98928517b293975dd70be4635eb7271f75c6644acd2769159674`.
The test parses the published research layout with a fixed, hard-coded
salt/nonce/slot structure and fixed KDF parameter limits. There is
**no format-version auto-upgrade** and no silent fallback to plaintext
or a machine-local encryption key.

## Synthetic scenarios

1. Verify fixture encoding, exact byte length and pinned SHA-256.
2. Create a sealed copy in a disposable temporary source tree, copy
   ciphertext to an unrelated destination tree with a different path,
   then read and independently decrypt that copy with the synthetic
   fixture password. No recovered plaintext is saved to disk.
3. Compare the restored payload to **independent expected public bytes**;
   verify both encrypted copies are still byte-for-byte identical to
   the pinned original after the operation.
4. Wrong password, modified wrapped key, and modified ciphertext
   fail authentication. No unauthenticated plaintext is returned and
   the encrypted source remains unchanged.
5. Hostile version, password slot, Argon2id cost/memory values,
   truncation and extra bytes are **rejected before KDF**. Input/output
   aliasing is rejected before mutating ciphertext.

The target is registered only when `RB_BUILD_PORTABLE_CRYPTO_PROBE`
is enabled, as the **16th CTest group** on Windows and Linux. It is
not installed, not packaged, and does not enter a public release.

## Important limits

- **This is not a physical cross-device transfer test.** CI on Windows
  and Linux is useful compatibility evidence but not validation on
  two real end-user computers or separate account profiles.
- Both decoders share the **libsodium primitives** and the same
  experimental layout. This is an independent *parser/decryptor*
  path and fixture, **not** an independent third-party crypto audit.
- The existing candidate memory-lock policies are **not** used here.
  The new decoder only tests portable ciphertext meaning; it is not
  an implementation of secure memory locking or GUI consent.
- It uses ordinary Qt byte buffers for the **public synthetic**
  plaintext; therefore no claim about zeroing all plaintext memory
  or resistance to machine compromise is justified.
- The fixed public fixture salt, key/password and nonces are
  deliberately unsuitable for live data.
- Tests do **not** verify OS keychain recovery, password normalization
  across different locale settings, GUI flow, write durability under
  power loss, actual cloud backup corruption, rollback, or larger
  hostile files. Those are separate future release gates.

## Safeguard

This PR remains **DRAFT** and must not be merged or used to enable
production encryption without explicit approval and independent review.
