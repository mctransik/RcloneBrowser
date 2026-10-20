# Portable saved-task encryption: password recovery + optional Windows quick unlock

**Status:** design proposal / security review required (2026-10-20).
**Decision:** password-based portability is the foundation; Windows DPAPI is a
*user-opt-in convenience mechanism*, **never the only way** to recover the task
data. This supersedes the proposed DPAPI-only default in
`TASK_ENCRYPTION_DESIGN_2026-10-17.md`.

**IMPORTANT:** No password encryption or migration is implemented by this
document. Current `tasks.bin` continues to use the unchanged legacy
`QDataStream::Qt_5_2` format. Do not ship, migrate user files, or promise
security until independently reviewed and tested.

## Product behavior and choice

1. A user explicitly creates an encryption **main password** before
   migrating any saved tasks. The password must unlock the same protected
   `tasks.bin` after an OS reinstall or on another supported computer.
   The application cannot recover a forgotten password; a **previously
   enabled** DPAPI quick-unlock slot may still work on its original Windows
   account but is not a substitute for a portable backup and the password.
2. Optional checkbox in **non-portable Windows** mode: "Unlock on this Windows
   account without asking for the main password." Default **off** until
   evaluated in user testing. The main-password unlock slot always remains.
   Enabling it requires an already unlocked task key and explicit user action.
3. In Windows **portable mode** or on Linux/macOS, do not offer DPAPI quick
   unlock: it undermines portable expectations and is unavailable on other OSes.
   Open via main password. Avoid binding the file to a Windows account by
   accident when the user switches modes.
4. "Disable quick unlock" removes the local DPAPI convenience wrapping on
   the next authenticated write without invalidating password recovery.
   If a DPAPI wrapping is stored in the file, explain that older copies can
   still contain it. If stored separately, that local artifact also needs safe
   lifecycle management.
5. Wrong password, changed Windows user, malformed data, too-expensive KDF
   parameters or failed decryption must trigger a clear **locked/read-only**
   state. Never replace a protected file with empty/legacy plaintext, never
   silently fall back to no encryption and never emit passwords, keys, URL
   tokens or task paths in logs/error telemetry.
6. Prior to first encryption, show a backup/recovery explanation and ask
   for explicit confirmation. No unattended automatic plaintext migration.
   Verify plaintext serialization, encrypted write, re-read/decrypt and full
   task decode before considering a migration successful. Preserve the
   original file for rollback with a secure *user-controlled* backup policy;
   plainly warn that a prior plaintext backup remains unencrypted.
7. Document that `rclone.conf`, arbitrary `--config` files, transfer logs,
   temporary files and cloud credentials elsewhere are **not** protected just
   because `tasks.bin` is encrypted.

## Proposed cryptographic architecture (not yet implemented)

Use a vetted, version-pinned distribution of **libsodium** on Windows and
Linux with dependency provenance, licenses, update policy and runtime-loading
audits. Do not implement Argon2, XChaCha20 or authentication primitives
ourselves.

- Generate a fresh independent **256-bit data encryption key (DEK)** using
  libsodium CSPRNG. Encrypt the complete serialized Qt task stream using
  `crypto_aead_xchacha20poly1305_ietf_*` with a fresh 192-bit nonce and
  an authentication tag.
- Derive a **password wrapping key (KEK)** from the main password using
  `crypto_pwhash(..., crypto_pwhash_ALG_ARGON2ID13)` with a fresh random salt
  and *explicitly stored* algorithm/parameter identifiers. Use the
  32-byte KEK to AEAD-wrap the DEK with its **own independent nonce**.
  Never use the human password directly as an AEAD key.
- The first implementation should benchmark the
  `crypto_pwhash_OPSLIMIT_MODERATE` and `MEMLIMIT_MODERATE` settings
  (currently 256 MiB) on the target machines. If too expensive for supported
  machines, adjust **before locking the file format**. Never silently lower
  security to accommodate a runtime failure. On read, impose strict bounds on
  stored work factors *before* allocating memory or starting Argon2id; reject
  absurd or below-minimum parameters.
- A separately enabled Windows DPAPI slot **wraps the same random DEK**, using
  `CryptProtectData` in current-user scope, `CRYPTPROTECT_UI_FORBIDDEN`,
  context binding and suitable authenticated metadata. Do not use machine-wide
  DPAPI or use DPAPI to derive the portable key. A DPAPI failure leaves the
  password slot usable.
- Do not store the raw DEK, password or KEK in the file, configuration or logs.
  Zero sensitive temporary memory where supported; document Qt's copy/
  implicit-sharing limitations. Re-encrypt payloads with fresh nonces on
  writes and validate all buffers/lengths before allocations.

### Versioning and file separation

Reserve the eight-byte prefix **`RBTSENC2`** for the future portable,
password-recoverable envelope. Existing `RBTSENC1` continues to mean the
DPAPI-only *prototype* envelope, which is not enabled for automatic
persistent migration. Prefix `RBTSENC*` future/unknown formats are
treated as encrypted/unsupported, not legacy plaintext.

**Reserved does not mean supported.** Until a full authenticated v2 parser,
encryptor, decryptor, migration and regression suite exists, the actual
task-file loader must refuse `RBTSENC2` and stay read-only. Never write a
fake v2 header with plaintext following it.

Freeze an exact canonical v2 binary layout only after a separate reviewed
proposal with explicit integer endianness, versioned authenticated fields,
bounded sizes and no JSON ambiguities. At minimum include:

- format version, KDF algorithm/version/salt/work factors,
- per-file identifier and bounded key-wrapping slot collection,
- fresh AEAD nonces and authenticated ciphertext/tag for task payload and
  wrapped DEK,
- authentication of security-relevant header metadata with domain-separated
  associated data (including file identifier, format version and KDF settings).

Adding/removing a wrapping slot must not allow swapping it onto another file,
switching algorithms, downgrading KDF parameters or changing the file format
without authenticated validation. Avoid key/nonce reuse, accidental
authentication bypass and unbounded attacker-controlled allocations. Decide
whether slot changes rewrite and verify the whole envelope atomically.

## Recovery and threat boundaries

**Supported objective:** copy encrypted `tasks.bin` to another computer,
enter the main password, and restore all saved task metadata without any
Windows-account dependency.

**Not guaranteed:** restore without the main password/another working unlock
method, detect a full rollback to an older *valid* file without trusted
external state, prevent malware running as the unlocked user from accessing
in-memory keys/tasks, protect the user's `rclone.conf`, avoid weak password
guessing by an offline attacker, or repair damaged/lost files without backups.

An attacker who obtains the encrypted file can attempt password guessing
offline. Recommend a strong, memorable password and a safe backup plan.
DPAPI quick unlock makes tasks accessible to programs acting with the same
Windows-user privileges; tell users this trade-off before enabling.

## CI / delivery gates

- [ ] Explicit v2-format *detection/refusal* regression tests on Windows
      and Linux, without enabling v2 decryption or writing.
- [ ] Dependency choice and reproducible pinned libsodium package/CI
      integration plus license and SBOM inventories.
- [ ] Full v2 envelope binary specification, independent security review and
      adversarial parser fuzzing.
- [ ] Tested password-only round trips and wrong-password/tamper rejection,
      empty task list, historical stream versions, changed work factors.
- [ ] DPAPI slot add/remove/recovery tests with one and two Windows user
      profiles; demonstrate portable recovery when DPAPI unavailable.
- [ ] Cross-OS encrypted-file portability tests with synthetic credentials.
- [ ] Transactional migration with rollback, secure plaintext backup handling,
      no silent downgrade and fail-closed recovery in all paths.
- [ ] Windows GUI walkthrough for both choices, local accounts and portable
      mode, plus restart/reinstall simulations with *synthetic files only*.
- [ ] Resolve outstanding malware classification, provenance, Authenticode,
      third-party license/attribution issues and all other release gates.

## Relevant upstream API documentation

- https://doc.libsodium.org/password_hashing/default_phf
- https://doc.libsodium.org/doc/secret-key_cryptography/aead/chacha20-poly1305/xchacha20-poly1305_construction

No public release until the cryptographic integration and recovery behavior
are demonstrated and independently reviewed.
