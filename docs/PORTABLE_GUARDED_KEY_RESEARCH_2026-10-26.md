# Isolated libsodium guarded-key owner: research review (2026-10-26)

**RESEARCH ONLY — not production-safe or integrated.**
This patch builds a **separate QtTest executable** named
`test_portable_guarded_key` behind
`RB_BUILD_PORTABLE_CRYPTO_PROBE`. It is not linked into the application, has
no install rule and does not change `tasks.bin`, the existing synthetic
`RBTSENC2` fixture, the password GUI or the Windows release ZIP.

## Why this experiment follows PR #20

PR #20 added scope-exit `sodium_memzero()` as a backstop for stack-based
synthetic KEK/DEK arrays. That helped error paths but did not address
ordinary C++ key copies, swap behavior, or pages exposed between uses.

This experiment investigates a **different approach** in isolation:
one owning C++ object holds a pointer, and the 32 synthetic bytes live
only in a libsodium guarded allocation. The owning object has deleted
copy and move constructors and assignments. It does not own any Qt
`QString` or `QByteArray` secret buffers.

## Precise rules tested

- Call `sodium_init()` before `sodium_malloc(32)`. Treat a null result as
  **allocation failure**, not a signal to fall back to ordinary heap
  allocation, copy a secret elsewhere or reduce KDF cost.
- After successful allocation, explicitly clear libsodium's
  nonzero initial fill with `sodium_memzero()`. No uninitialized
  bytes are ever intentionally used as a cryptographic key.
- The *single owner* calls `sodium_free()` exactly once at scope exit.
  libsodium checks the allocation canary and wipes/releases it. The
  test deliberately **does not inspect freed memory**.
- Deliberately injected allocation failure checks that no owner
  is created and no free function is called. A counting allocator
  and deallocator check the normal one-allocation/one-release path.
  C++ owner-allocation failure is guarded in the implementation too,
  but not forcibly simulated by the test.
- `sodium_mprotect_readonly()`,
  `sodium_mprotect_noaccess()` and
  `sodium_mprotect_readwrite()` are exercised where the
  host platform permits them. Accessors return null for disallowed
  modes. **Tests never intentionally dereference protected memory.**
- Injected protection failures leave the recorded mode unchanged
  and return false so the caller can abort instead of treating a
  failed hardening request as successful.
- QtTest skips the *optional page-protection portion* only if the
  environment cannot support it. This is not proof of protection in
  every end-user environment. A future production policy that
  **requires** inaccessible pages must reject the operation when
  that protection cannot be established.

## Limits: what these tests do NOT prove

- No real user password, session key, task data or backup is handled.
  All bytes are public synthetic constants.
- The guards do not prevent CPU-register/stack copies, kernel snapshots,
  privileged memory readers, crash dump leaks or every swap scenario.
  `sodium_malloc()` **attempts** to lock allocated memory;
  OS limits can cause locking to fail without making the allocation fail.
  **Do not claim memory was locked** without an explicit separately
  checked locking policy and tests.
- The test's raw pointer accessors are only valid while the owner
  exists, and a caller could misuse or retain a pointer. They do not
  establish a general production-safe borrowing or callback API.
- There is no integration with the existing `ScopedKeyWipe` stack
  owner or `RBTSENC2` key lifecycle, so existing encryption
  research remains untouched. Future integration would need careful
  conversion with no transient extra secret copies.
- The tests cannot guarantee protection if `sodium_mprotect_*()`
  changes an underlying page but still reports an error, nor if a
  third-party injected allocator violates libsodium's contract.
  The injection seam is solely for deterministic negative-path tests.
- Guarded allocations cost additional virtual-memory pages per key.
  They are **not** appropriate for allocating the 256-MiB Argon2id
  working memory. No KDF calibration runs in CI.
- No password normalization, versioned Unicode policy, serialization
  migrations, backup rollback, or real-device recovery is authorized.

## Gate before touching application code

Document and review a threat model; decide whether page protection and
page locking are required (and what happens if unavailable); review
borrowed-pointer lifetimes and secure-input/output boundaries; provide
independent audit plus fuzzing, cross-platform recovery tests, crash/
rollback handling, secure backup retention and explicit user opt-in.

Upstream libsodium memory contract:
https://libsodium.gitbook.io/doc/memory_management

**The PR stays DRAFT and unmerged. Release must remain disabled.**
