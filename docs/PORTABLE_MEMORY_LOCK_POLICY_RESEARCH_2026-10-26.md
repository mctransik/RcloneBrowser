# Memory-locking availability: recovery policy research (2026-10-26)

**RESEARCH ONLY.** No production policy has been selected. This branch does
not modify real passwords, user tasks, encrypted files, login UI, actual
`RBTSENC2` wire format, nor the production build.

## The verified upstream behavior

The official libsodium memory-management documentation explicitly notes:

- `sodium_malloc()` attempts memory locking, but **can still return a valid
  allocation even if `mlock()`/`VirtualLock()` fails or is unsupported**.
  Allocation success must never be displayed as **confirmed locking**.
- `sodium_mlock()` reports failure when the operating system denies a lock.
  Available locked memory varies with system configuration and limits.
- `sodium_munlock()` overwrites the specified bytes before unlocking them.
- Guard pages and `sodium_mprotect_*()` are separate from page locking:
  **memory access restrictions are not proof against paging**.
- Neither locked pages nor access permissions protect register/stack copies,
  all crash/hibernation images, malware, or privileged inspection.

Source: https://doc.libsodium.org/memory_management

## Candidate policies (NO decision yet)

The new **pure test-only** `evaluate()` function models three options:

| Lock outcome | Strict | Best effort | Recovery-only |
| --- | --- | --- | --- |
| Lock never checked | **Reject** | **Reject** | **Reject** |
| Lock explicitly succeeded | Allow; locking verified | Allow; locking verified | Allow; locking verified |
| Lock explicitly failed during **new backup creation/migration** | **Reject** | Allow, **degraded** | **Reject** |
| Lock explicitly failed during **restoration of an existing backup** | **Reject** | Allow, **degraded** | Allow, **degraded** |

A degraded result must **never** claim the pages are locked. In an actual
product, degraded recovery would additionally require a clearly defined
threat model and user-facing security treatment, and a strict policy could
block users from restoring their own valid backups.

**Potential direction for further review, not authorization:** a
recovery-only exception may reconcile locked-memory preferences for *new*
data with portability when unlocking *existing* archives. It is not
necessarily safer overall: denying safe recovery can itself cause data
loss, drive insecure manual workarounds or strand users. The tradeoff
must be deliberately reviewed and tested; no default has been chosen.

## Implementation and test boundaries

- The new fixture `tests/test_portable_memory_lock_policy.cpp` tests
  **all three options** with deterministic success/failure/not-attempted
  values and checks that **no unverified path claims locking**.
- A separate optional OS probe calls `sodium_mlock()` and
  `sodium_munlock()` on a disposable **plain heap** buffer
  holding only public synthetic bytes. The release call is checked
  and the zeroed bytes are verified while the allocation is still live.
- The probe **does not explicitly call `sodium_mlock()` on a
  `sodium_malloc()` region**, because that memory may already
  be locked by the allocator. Re-locking it without a reviewed matching
  release strategy risks incorrect lock accounting and mistaken assurances.
  The previous guarded-owner fixture remains unchanged.
- An OS lock denial causes **only the optional environment probe** to
  be skipped; the deterministic policy table runs regardless. A green
  test run does not prove that page locking works on every end-user PC.
- Under CI, the test is separate from the actual application and is
  the **15th top-level CTest group** on Windows and Linux.
- This does not specify or enforce any new product access control.
  The test's simulated decision is not an authorization mechanism.
- No runtime flags, OS policy, registry configuration or privilege
  changes are introduced. No passwords, file names or machine identifiers
  are collected.

## Review questions before a production decision

1. What are the product's realistic threats: **offline theft of backups**
   versus a **compromised running machine**, paging/swap, and crash dumps?
2. Should the user be able to restore existing password-encrypted files
   on a system where locking is denied? How should the limitation be
   communicated without creating confusing or repeated warnings?
3. Does forcing strict locking impose unacceptable accessibility or
   operational consequences on Linux desktops, Windows configurations,
   containers, or low-resource devices?
4. Which operations can safely switch access protection in and out?
   Does a failed `sodium_mprotect_*()` call cause a fatal error,
   or is there a genuinely safe fallback?
5. How would a real implementation validate its lock status **without
   double-locking `sodium_malloc()` memory** or misusing a borrowed pointer?
6. How will independent security review cover input/password copies,
   normalization, secure buffers, rollback, cross-OS recovery and
   user-authorized migration?

### Non-negotiable release gates

Do not merge, change production `tasks.bin`, turn on encryption,
or publish a release as a result of this research. CI checks here only
validate synthetic unit behavior and a limited optional OS API probe.
