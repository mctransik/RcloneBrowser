# Research: recovery authorization with unavailable OS memory locking (2026-10-26)

**Synthetic test fixtures ONLY. No production encryption, real passwords, user tasks or `tasks.bin`. No releases.** This is an *unapproved* research candidate stacked on PR #22.

## Why this work matters

A password-encrypted backup must remain recoverable after a computer fails
or after the owner changes operating systems. A second computer may have
different OS restrictions on memory locking. We must not silently pretend
that missing memory protection succeeded, but we also must not casually
strand a legitimate backup during recovery.

PR #22 investigated a deterministic three-way candidate matrix for
`Strict`, `BestEffort`, and `RecoveryOnly` locking decisions. This PR
**shares that same matrix between the policy tests and actual synthetic
fixture-decryption tests**, instead of copying the decision logic twice.

## Implemented as isolated CTest research

- `tests/portable_recovery_lock_policy.h` is the **one test-only source**
  for the lock-decision enums and `evaluate()`. No application linkage.
- `tests/test_portable_memory_lock_policy.cpp` now reuses the header;
  the prior deterministic matrix and optional real-OS API test remain.
- `tests/test_portable_v2_envelope.cpp` gains a **test-only**
  `researchRestoreExistingFixture()` wrapper that:
  1. checks output/input aliasing first, preserving the source bytes;
  2. refuses unattempted memory locking or any policy-denied attempt
     **before calling Argon2id**;
  3. for a policy that conditionally permits degraded recovery, requires
     **a separately supplied explicit acknowledgement**; absence of
     acknowledgement refuses recovery without calculating a key;
  4. calls the existing authenticated research decoder only after the
     preceding checks; rejects wrong passwords and tampering without
     returning unauthenticated plaintext;
  5. reports `degraded=true` whenever OS locking was simulated as
     unavailable. It never claims that such pages were locked.

### Tests

- Read **the already-committed independent 202-byte `RBTSENC2` file
  fixture**, write an encrypted copy to a disposable source directory,
  copy it into a separate disposable destination directory and load
  its bytes from disk. Both directories live under `QTemporaryDir`.
- Under a simulated lock failure, `Strict` recovery **rejects**.
  When locking was not attempted, **all** policies reject.
- Under `RecoveryOnly` and `BestEffort`, **degraded restoration**
  requires separate acknowledgement; after explicit acknowledgement,
  the unchanged fixture can be authenticated and recovered.
- When the simulated OS lock **succeeded**, the `Strict` candidate
  can recover without degraded acknowledgement.
- Wrong password, tampered ciphertext and malformed KDF metadata are
  rejected; output remains empty; encrypted original on disk is not
  rewritten or deleted.
- Aliased caller output to either the ciphertext or password is
  rejected **before any output buffer mutation**.
- `RecoveryOnly` never permits **creating or migrating new archives**
  if OS memory locking was denied or never verified.
- The old independent hex fixture and wire format remain unchanged.

**No real cross-device transfer has been tested by this patch.**
Windows and Linux executing the same committed independent fixture
is useful evidence of shared interpretation, **not** proof that a file
copied over the network or between real end-user machines will behave
correctly under every configuration. CI should still run 15 top-level
CTest groups per platform, including these added subcases.

## Strong limitations / review gates

1. The lock state is **injected test data**, NOT the output of an actual
   guard or `mlock` call in the recovery wrapper. `sodium_malloc()`
   can return success without actual page locking, so production would
   require its own independently reviewed *verified-lock* mechanism
   before using any corresponding decision.
2. The acknowledgement is a **Boolean supplied by a test**. No GUI
   warning, user action, consent mechanism or recovery workflow exists.
   Nobody should interpret passing tests as permission to silently
   operate with degraded protection.
3. The wrapper operates **only on in-memory synthetic ciphertext**
   loaded by the test. It neither writes recovered plaintext to disk
   nor migrates real tasks nor implements crash-safe file replacement.
   `QByteArray::clear()` does **not** guarantee that all plaintext copies
   were physically erased from RAM.
4. The shared policy header has **no versioned on-disk semantics**.
   Its decision matrix is a design candidate, not a production security
   setting and must not change the meaning of existing user backups.
5. Users can lose access to existing backups if the application
   unnecessarily depends on an OS-specific account, a mandatory
   memory-lock facility or an incompatible password normalization.
   Password-only portability, independent source verification and
   offline recovery need dedicated review.
6. CI cannot prove the lack of a vulnerability. Further requirements
   include third-party crypto review, parser fuzzing, fixed cross-OS
   vectors and actual backup transfers, verified transactional
   migration and rollback, restore UI, authenticated file checks and
   independent user-controlled recovery copies.

Related documents:
- `docs/PORTABLE_MEMORY_LOCK_POLICY_RESEARCH_2026-10-26.md`
- `docs/PORTABLE_GUARDED_KEY_RESEARCH_2026-10-26.md`
- `docs/PORTABLE_RECOVERY_FIXTURE_REVIEW_2026-10-23.md`

**Production task encryption remains OFF; the PR remains DRAFT.**
