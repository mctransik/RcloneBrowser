# Portable password bytes: Unicode normalization candidate (2026-10-24)

**RESEARCH ONLY — NOT an approved password policy.** This work is entirely
within the isolated `test_portable_v2_envelope` CTest target. No app GUI,
normal task-file loader, `tasks.bin`, or production encrypted backup is changed.

## Why this is necessary

The existing experimental `RBTSENC2` codec accepts raw `QByteArray`
password bytes, while a future graphical password dialog will produce a
Unicode `QString` (UTF-16 on Qt). Two passwords can *look identical* yet
have different bytes on disk. For instance:

| Synthetic password | Code points | Raw UTF-8 (hex) |
| --- | --- | --- |
| `Café` (precomposed) | `U+0043 U+0061 U+0066 U+00E9` | `436166c3a9` |
| `Café` (combining accent) | `U+0043 U+0061 U+0066 U+0065 U+0301` | `43616665cc81` |

If the UI treats these different byte strings as the *same* password in
one code path but not another, an otherwise valid encrypted backup may
become inaccessible on another operating system or after an update.

## Candidate: explicit NFC -> UTF-8, bounded

A new **test-only** function `researchNfcPasswordBytes()` explores:

1. Accept `QString` input; **reject** empty input, embedded U+0000, any
   unpaired UTF-16 surrogate (rather than silently replacing it), and
   excessively long UTF-16 input *before* normalization.
2. Normalize with Qt `QString::NormalizationForm_C` (NFC); convert the
   result explicitly to UTF-8 bytes.
3. Enforce a **4096-byte maximum** on the encoded UTF-8 output; never
   infer the byte count from displayed characters.
4. Preserve Unicode compatibility distinctions (NFC does **not** convert
   circled `①` to `1`, unlike NFKC). Valid supplementary Unicode
   characters, e.g. a lock emoji, must encode correctly.
5. Reject malformed input **before** calling the memory-hard KDF.
   Current CTest counters verify that no Argon2id call occurs for any
   invalid input in this experiment.

All test inputs are fictional literals in the public source. They are not
real credentials. The current raw-byte `RBTSENC2` encoder/decoder remains
unchanged. The older deterministic file vector remains unchanged.

## What the tests can and cannot establish

Windows and Linux CI should execute these same candidate tests. The
synthetic tests assert that visually identical `Café` spellings
normalize to the same UTF-8 bytes and can unlock a disposable encrypted
fixture when **explicitly passed** to the codec.

But **this candidate is not automatically used for real passwords**.
It does not decide that NFC is preferable to preserving raw password
bytes or to any other independently reviewed policy. It does not
guarantee that display, input-method editors, paste, keyboard layouts,
Unicode tables, Qt version upgrades, password-manager autofill, and
password recovery behave identically across all supported devices.

## Remaining design/security decisions

- Choose a **versioned password-byte profile** for each actual wire
  format or password slot, including the Unicode normalization form,
  accepted scalar values, input and output size limits, and what to
  display to users. No silent changes to existing encrypted backups.
- Define behavior for replacement characters from invalid UTF-8
  decoding; at the `QString` boundary alone, literal U+FFFD cannot be
  reliably distinguished from a lossy earlier conversion.
- Test on differing Qt/Unicode builds; pin external test vectors and
  review normalization stability and Unicode-data compatibility.
- Decide if the 4096-byte upper bound is acceptable for password
  manager workflows, and define a sensible *minimum* UX warning without
  silently truncating input.
- **Security-critical:** Qt `QString` and `QByteArray` are implicitly
  shared and may leave multiple plaintext copies. This helper does
  not yet use independently reviewed secure memory, key erasure, or
  secret-handling lifetimes. Do not integrate it into a production GUI.
- Avoid password or key material in logs, telemetry, CI artifacts,
  screenshots, or crash dumps; a **synthetic** UTF-8 vector is public.
- The preliminary 64/256-MiB Argon2id timings observed on a developer PC
  are useful for exploration but **must not be treated as approval** of
  a production KDF. No identifying machine details or private timings
  are included in this publicly readable review document.
- Carry out independent security review, mutation/fuzz tests, migration
  rollback, real cross-OS backup transfer tests, and explicit user
  opt-in before activating any real encryption.

See also:
- `docs/PORTABLE_KDF_POLICY_BENCHMARK_2026-10-21.md`
- `docs/PORTABLE_RECOVERY_FIXTURE_REVIEW_2026-10-23.md`
- `docs/PORTABLE_V2_ENVELOPE_WIRE_CANDIDATE_2026-10-21.md`

**No production encryption or release is authorized by this PR.**
