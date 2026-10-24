# Research review: encryption-key lifetime and cleanup guards (2026-10-24)

**Synthetic experiments only. Not integrated into the RcloneBrowser GUI or
`tasks.bin`. No production key-handling or encryption approval.**

## Concrete audit findings

The existing `RBTSENC2` proof of concept uses `std::array<unsigned char, 32>`
for the **key-encryption key** (KEK, derived by Argon2id13) and **data
encryption key** (DEK). It already called `sodium_memzero()` immediately
after key usage in most successful and authentication-failure paths.
However, some *early exits* (for example, a failed `crypto_pwhash()`
derivation) returned before a guaranteed wipe of **both** local key
buffers. This is a risk to address before handling actual credentials,
even though current CI uses artificial public strings and keys.

The code also cleared output buffers before detecting two accidental
input/output alias classes: `seal(..., password, &password)` and
`open(..., password, &password)`. These now return **without clearing
the original bytes**. Prior tests already covered aliases to the
plaintext or encrypted input object. Note: this checks **object identity**,
not the possibility of multiple Qt objects sharing the same allocation.

## Research-only hardening

The isolated `tests/test_portable_v2_envelope.cpp` now defines
`ScopedKeyWipe`. It is a **non-copyable, non-movable** scope-exit
guard around an existing fixed-size key buffer. Its destructor invokes
libsodium's `sodium_memzero()` for the **exact memory address and byte
length** supplied by the owner. This mitigates omissions when a later
code edit adds a return statement.

- The local KEK and DEK in `open()` always have guards once allocated.
- The local KEK and the selected synthetic DEK in `seal()` are similarly
  guarded; the guard lives **after** the corresponding buffer is created,
  and ends **before** the buffer's storage lifetime ends.
- **Immediate wipes at last use remain**: after wrapping with KEK,
  after payload encryption with DEK, after unwrapping with KEK and after
  payload decryption with DEK. The scope guard may wipe the same buffer
  again at function exit: this is intentional defense in depth.
- Derivation failure and other early returns also leave via the scope
  guards. These paths **must never auto-downgrade** KDF cost or return
  unauthenticated plaintext.

An instrumentation counter `keyWipeGuardsRun` exists *only* in this
single-threaded test binary. New QtTest cases demonstrate that a
nonzero **invented** key becomes all zero at scope exit and that the
guards run on successful sealing/opening and on wrong-password and
tampered-ciphertext failures. They check that invalid headers are
refused without initializing local keys, while old format vectors
continue to pass. Tests also reject accidental password/output
object aliasing before mutation.

**Important:** Counting destructor calls is NOT a general proof of
secure deletion; it merely verifies some control-flow cases. The
separate byte-zeroing test only covers the specific local synthetic
buffer. Compiler-generated copies, stack spills, and copies in
libraries or the OS are not measured.

## Security gaps intentionally left unresolved

1. **Qt implicit sharing:** `QString` and `QByteArray` may share
   storage or create independent copies through conversions,
   normalization, UI controls and logging. `clear()` is **not**
   evidence that every backing allocation has been overwritten.
   In particular, clearing an output on failure does not prove that
   old plaintext bytes were erased from RAM.
2. **Research fixture copies:** `Inputs` is copyable and holds
   a *synthetic* DEK. In `seal()`, selecting a passed fixed fixture
   or constructing `randomInputs()` can create extra copies of key
   bytes. The guards cover **only the selected local buffers**,
   not all earlier or compiler-generated copies.
3. **Password-byte lifecycle:** The NFC-to-UTF-8 candidate returns a
   `QByteArray`; the GUI input and normalization pipeline is not
   reviewed for erasure or locked memory. No real passwords may be
   routed through this experimental helper.
4. **Plaintext staging:** Decrypted task bytes currently go into a
   mutable Qt `QByteArray`. Failure paths attempt to clear staged
   plaintext, but success transfers it to a caller with no reviewed
   ownership or erasure policy. No guarantee about paging, crash dumps,
   suspend/hibernation, memory snapshots, or malware with process access.
5. **Guarded memory:** libsodium documents `sodium_malloc()`,
   `sodium_free()` and `sodium_mlock()`. They may improve defensive
   handling of fixed keys, but allocation/locking can fail and
   memory protection adds overhead and platform constraints. Do **not**
   casually add page locks, call `sodium_free()` on Qt-managed buffers,
   or assume locked memory defeats a live process compromise.
6. **Threat model:** At-rest backup confidentiality, strong password
   recovery and fail-closed parsing are separate from defending
   against an attacker controlling the running computer. No local
   memory technique can guarantee protection against every local
   privileged or memory-inspection attacker.

## Follow-up / release gates

- Security review of key ownership, copies, failure cleanup, and UI
  password lifetime before linking any libsodium functionality into
  production.
- Explore a **fixed-size noncopyable secret owner** and safe
  libsodium-guarded allocation in a separate test-only PR;
  explicitly test allocation failure and key lifetime contracts.
- Add independent format/parser fuzzing and actual cross-OS transfer.
- Select KDF and Unicode profiles only after review. The first observed
  local hardware measurements are not a production KDF decision.
- Require user approval, tested backup retention and atomic migration
  before touching any real `tasks.bin`.

Upstream reference: https://doc.libsodium.org/memory_management

**This research branch remains draft, unmerged and non-releasable.**
