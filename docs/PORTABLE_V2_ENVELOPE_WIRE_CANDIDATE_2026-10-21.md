# Experimental RBTSENC2 portable envelope — wire-format candidate

**Date:** 2026-10-21
**Status:** research-only candidate. **NOT a production format or a migration
path**. Not linked into RcloneBrowser, not connected to `tasks.bin` loading or
writing, not included in distributable archives. Its behavior **must be
reviewed before this layout or its algorithms are frozen**.

Implements a synthetic in-memory **password-only** fixture in
`tests/test_portable_v2_envelope.cpp`, compiled only when
`RB_BUILD_PORTABLE_CRYPTO_PROBE=ON`. It uses the already tested libsodium
integration from PR #14. No new application dependency is introduced.

## Canonical candidate layout

All integers are **unsigned, big-endian, exactly the widths specified below**.
Offsets count from the start of the binary buffer. No Qt stream encoding,
platform-dependent padding, text parsing, or implicit terminators.

| Offsets | Size | Field | Allowed value in this experiment |
| --- | ---: | --- | --- |
| 0–7 | 8 | Magic | ASCII `RBTSENC2` |
| 8 | 1 | Draft format revision | `1` |
| 9 | 1 | Payload/wrapping AEAD | `1` = XChaCha20-Poly1305 IETF |
| 10 | 1 | Password KDF | `1` = Argon2id13 |
| 11 | 1 | Number/type of slots | `1` = single password slot only |
| 12–15 | 4 | Argon2id operations | `2` (development INTERACTIVE) |
| 16–19 | 4 | Argon2id memory (KiB) | `65536` = 64 MiB (development only) |
| 20–35 | 16 | File identifier | CSPRNG random |
| 36–51 | 16 | Argon2id salt | CSPRNG random |
| 52–75 | 24 | Wrapped-key nonce | CSPRNG random |
| 76–99 | 24 | Task-payload nonce | CSPRNG random |
| 100–101 | 2 | Wrapped DEK ciphertext length | `48` bytes fixed |
| 102–105 | 4 | Task-payload ciphertext length | 16–1,048,592 bytes in fixture |
| 106–153 | 48 | Encrypted DEK + 16-byte AEAD tag | 32-byte random DEK |
| 154–end | payload length | Task ciphertext + 16-byte tag | Opaque bytes |

The **entire header** (bytes 0–105) is associated data for DEK wrapping.
The **entire header and 48-byte encrypted DEK** (bytes 0–153) are associated
data for the payload encryption. The associated data is prefixed with
**distinct ASCII context strings**:

- `RcloneBrowser/RBTSENC2/wrap/1`
- `RcloneBrowser/RBTSENC2/payload/1`

Tampering with the version, algorithm, work factors, slot count, file ID,
salt, nonces, sizes, or key material must cause decryption to fail closed.
Swapping the wrapped DEK or ciphertext across otherwise valid files must fail
because the whole relevant framing is authenticated. Any future slot
addition/removal **will require intentional rewriting and reauthentication**;
the current single-slot layout cannot represent DPAPI or extra slots.

### Parser policy and resource bounds

The experimental parser requires exactly:
- 8-byte magic and recognized revision, AEAD, KDF and slot identifiers.
- Fixed, **exact** development Argon2id work factors (`2` / `65536 KiB`).
  Any other value is rejected **before any Argon2 operation**. Production
  parameters must be benchmarked/approved and bounded before a release.
- Fixed 48-byte wrapped-key ciphertext, at least the 16-byte AEAD tag for an
  empty plaintext, and **no trailing bytes**. Reject truncation and all
  inconsistencies between the declared and actual length.
- An overall fixture cap of **1 MiB plaintext** and 1,048,746 bytes total.
  This is intentionally less than the existing 64 MiB task read bound, not
  an accepted product limit. Revisit under authenticated/memory-safe design.

No plaintext may be returned on any error, including a previously populated
output buffer. The test-only function never accesses actual user files.

## Cross-platform deterministic regression vector

This vector is **public, invented test data only**. It must not be used to
encrypt any live data because it intentionally fixes the key, salt and nonces.

- Password UTF-8 bytes: `synthetic-fixture-password`
- Plaintext ASCII bytes: `Synthetic portable task bytes v2` (32 bytes)
- File ID: sixteen `11` hexadecimal bytes.
- Salt: sixteen `22` hexadecimal bytes.
- Wrapped-key nonce: twenty-four `33` hexadecimal bytes.
- Payload nonce: twenty-four `44` hexadecimal bytes.
- Data encryption key: thirty-two `55` hexadecimal bytes.
- Expected complete envelope length: **202 bytes**.
- Expected SHA-256 of complete envelope:
  `6a166dd34aeb98928517b293975dd70be4635eb7271f75c6644acd2769159674`.

This expected digest was computed **independently** with Python `ctypes`
calling system libsodium 1.0.18 and assembling the candidate big-endian
header, using the exact two associated-data domains. Qt/C++ Windows and
Linux builds must produce the *same* vector for those fixed inputs.
The test also checks normal randomized encryptions differ.

## Negative tests in this prototype

- Incorrect password produces no plaintext.
- Single-byte mutations to security-significant metadata, both AEAD
  ciphertexts/tags and nonces are rejected.
- Truncation at multiple positions, trailing bytes and impossible declared
  lengths are rejected before any large allocation or password hashing.
- Unapproved high KDF memory/operations are rejected before password hashing.
- Cross-file wrapped DEK and encrypted task payload swaps are rejected.
- Unknown magic and a simulated second (DPAPI) slot are rejected.
- Empty plaintext still has an authenticated AEAD tag and round-trips.

## What remains *before* production use

1. Independent cryptographic and parser review/fuzzing. Code under
   `tests/` is an isolated demonstration, **not an audited library**.
2. Decide an approved Argon2id work factor and support policy with benchmarks
   on actual Windows/Linux hardware; document password UTF-8 normalization
   and Unicode interoperability. Current demo has test-only low factors.
3. Decide whether the final v2 format should allow future slots, or use
   a new envelope revision. Specify DPAPI slot binding and portability.
4. Integrate **only after review** with atomic QSaveFile migration, staged
   rollback, reliable task deserialization, and locked/read-only recovery.
   Existing `tasks.bin` is plaintext and stays untouched in this branch.
5. Cross-OS real-file tests using synthetic tasks, backward-compatibility,
   corruption/fuzz, crash-atomicity and backup recovery.
6. Dependency provenance/SBOM/licenses, release warnings, signing and other
   separate security gates must be satisfied before shipping.

Related design:
`docs/PORTABLE_TASK_ENCRYPTION_PASSWORD_DPAPI_2026-10-20.md`.
