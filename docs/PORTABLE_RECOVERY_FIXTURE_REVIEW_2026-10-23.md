# Portable encrypted task recovery — synthetic file tests and password-byte gap

**2026-10-23 — experimental security investigation. No real task files used.**

This development-only addition builds on the unmerged, stacked draft PRs
for the experimental `RBTSENC2` wire codec. It only modifies the
`test_portable_v2_envelope` executable behind
`RB_BUILD_PORTABLE_CRYPTO_PROBE=ON`. It does **not** change the GUI, task
loader, format version, installed app binary, backup behavior or release
artifacts.

## New synthetic regression cases

1. **`encryptedSyntheticFileSurvivesPathMoveAndReload`** encrypts a
   made-up task string into a byte buffer, writes it atomically into an
   isolated `QTemporaryDir` using `QSaveFile` with direct-write fallback
   disabled, copies it into a **second isolated temporary directory**, removes the
   original test copy, reads the restored copy with an explicit byte cap, and
   decrypts it successfully using only the invented main password. The
   test checks that wrong-password attempts return **no plaintext** and
   do **not** alter or erase the encrypted file on disk.
2. **`cancelledSyntheticRewritePreservesProtectedFile`** creates a
   valid temporary encrypted fixture, starts a second atomic write with
   incomplete *synthetic* data, cancels it, then checks that the original
   encrypted bytes still exist and can be decrypted.
3. **`aliasedInputOutputNeverDestroysProtectedData`** refuses to
   encrypt/decrypt in place when the input and output `QByteArray`
   are the **same object**, avoiding accidental loss of the ciphertext or
   plaintext due to caller aliasing.
4. **`corruptedEncryptedDiskFixtureRemainsUnchangedOnFailure`** writes an
   intentionally tampered synthetic ciphertext to disk, then requires
   authentication failure to clear the proposed plaintext while leaving
   the corrupted file's bytes available for recovery and investigation.
5. **`unicodePasswordBytesExposeUnresolvedNormalizationPolicy`** shows
   that two visually equivalent Unicode strings (precomposed vs
   decomposed `é`) yield different raw UTF-8 password bytes. The
   current research codec **does not normalize passwords** and thus
   treats them as different. The test asserts this to prevent accidental
   claims of portable Unicode password recovery without an approved
   input-encoding contract.

The earlier cross-platform **deterministic SHA-256** test vector remains
unchanged. When both Windows and Linux pass, that demonstrates identical
envelope bytes for the **same fixed synthetic input**. These new tests
exercise synthetic file I/O on each platform; they do **not** physically
copy an encrypted file between two different operating systems. Do not
claim actual inter-machine recovery or a completed migration based on
these tests alone.

## Important limitations

- `tasks.bin` is still in its existing unencrypted legacy form. This
  branch does not read, migrate or protect user data.
- A successful synthetic `QSaveFile` test does **not** prove that
  production task migration is transactional, that a backup is safely
  retained, that legacy deserialization cannot fail, or that encryption
  is integrated into the actual task storage paths.
- The prototype is a **password-only** envelope. Optional Windows DPAPI
  quick unlock is a future convenience feature, not the only recovery
  mechanism, and is **not** implemented here.
- Argon2id INTERACTIVE (2 operations, 64 MiB) remains only a research
  fixture profile. The MODERATE (3 operations, 256 MiB) benchmark candidate
  requires measurement on representative **local target devices**, not
  hosted CI, and an independently reviewed minimum policy.
- A production password interface must explicitly define Unicode input
  behavior (normalization or preservation, rejected invalid scalar
  sequences, UTF-8 bytes, byte-length cap, handling of existing passwords
  after a future normalization-policy change). Never silently change the
  policy for an existing encrypted backup. Avoid logging passwords.
- Missing recovery passwords cannot be reconstructed from an encrypted
  file. Existing valid old encrypted files can be rolled back by an
  attacker with storage access unless separate trusted state exists.
- Synthetic negative-path tests are not a substitute for fuzzing, security
  review, actual cross-OS file copying, failure injection during real
  migrations, and independent backups.

## Review gate before turning on encryption

Agree on the production KDF parameters and password-byte policy; review
the canonical envelope format, parser, limits and nonce/key handling;
add cross-OS copy tests with fixed portable fixtures, then integrate
**only with explicit user confirmation**, verified atomic migration,
read-back, transaction-safe decode and recovery when decryption fails.
Never silently fall back to plaintext or overwrite a protected file on
any error.

Related:
- `docs/PORTABLE_V2_ENVELOPE_WIRE_CANDIDATE_2026-10-21.md`
- `docs/PORTABLE_KDF_POLICY_BENCHMARK_2026-10-21.md`
- https://doc.libsodium.org/password_hashing/default_phf
