# Transactional synthetic restore after authenticated recovery (2026-10-27)

**DRAFT SECURITY RESEARCH — NOT IN THE ACTUAL APP.** This change only extends
`tests/test_portable_independent_recovery.cpp`. No production tasks, backup
files, passwords, or `tasks.bin` are opened; there is no app integration,
backup migration, new release, or durable security policy decision.

## Motivation

The existing independent decoder in PR #24 verified that a synthetic
encrypted RBTSENC2 fixture can be read without a dependency on a source
file path or Windows account. However, returning synthetic plaintext in
memory is not enough to reason about safe restore to a destination file.
If a write is interrupted, the encrypted input and any already-existing
destination must not be accidentally destroyed.

## Added research-only restore stage

`researchRestoreAtomically()` first rejects an empty or obviously unsafe
destination, including the same path/canonical target as the encrypted
source, before starting a KDF. That is **not** a complete defense against
symlink/race attacks and must not be treated as one.

It then applies the **same test-only lock decision matrix** introduced in
PR #22 and reused in PR #23. A not-attempted lock is rejected. Simulated
lock failure under `Strict` is rejected; a degraded `RecoveryOnly`
candidate requires an explicit synthetic acknowledgement. No rejected
operation starts Argon2id.

Only after that gate does it use the independently implemented
`independentRead()` decoder from PR #24. This decoder authenticates the
wrapped key and ciphertext before returning synthetic plaintext. Wrong
passwords, unsupported headers, and tampering do not open a restore file.

On successful synthetic authentication, it creates a `QSaveFile` target
with `setDirectWriteFallback(false)`. Failed and explicitly cancelled
writes are discarded rather than overwriting an existing destination.
The test uses no real private content and never logs any user data.

## Added deterministic QtTest scenarios

- Already-existing plaintext output: an interrupted **partial write**
  must keep the previous destination byte-for-byte identical.
- Aborted after all bytes were staged but **before commit**: retain
  the existing destination unchanged.
- Successfully committed synthetic output replaces the prior destination;
  separately confirm the encrypted source is unchanged.
- Wrong password, authenticated tampering and invalid KDF parameters:
  no valid output; never replace the destination; malformed metadata
  rejected before Argon2id.
- Same source/destination path: reject before Argon2id.
- New output path: a cancelled partial write must not leave a half-
  written plaintext file, followed by a successful commit.

## Strict limits

1. **OS locking and consent are injected *simulation values*.** The wrapper
   is not an OS lock probe, isn't hooked to protected key memory, and has
   no real GUI approval. Do not conflate successful simulated locking
   with an actually mlocked page.
2. `QSaveFile` is a transactional write facility. Tests of deterministic
   cancellation do **not** prove real power-loss durability, disk flush/
   sync semantics, cross-filesystem atomicity, or resilience to a hostile
   path replacement/symlink race. Real-device fault injection and a
   carefully reviewed path/directory security policy remain necessary.
3. `QByteArray` ordinary storage is used because the payload is public.
   This is **not** a safe production plaintext staging design and does not
   prove zeroization of all plaintext copies.
4. The destination is synthetic. No arbitrary user-specified path or
   real user backup is used. Restore must eventually ensure destination
   ownership, directory permissions, space availability, external
   tampering, file identity and secure file naming.
5. Recovery and source ciphertext bytes currently arrive via separate
   arguments. The test confirms original ciphertext remains unchanged
   but does not implement a secure actual-file-load-and-write pipeline,
   nor enforce content/path identity against malicious caller input.
6. The experimental envelope is not an approved production file
   format. There is no release, migration, verified two-device restore,
   locked-memory guarantee or independent cryptographic audit.

This change adds **three QtTest cases inside the existing independent
recovery CTest suite**. The expected top-level CTest count remains **16**
on both Windows and Linux; the extra conditions are not a 17th suite.

**PR remains DRAFT and unmerged; release remains disabled.**
