# Saved transfer task encryption: staged design (2026-10-17)

> **Design superseded on 2026-10-20:** The earlier DPAPI-only default
> proposed below is no longer the planned default. See
> [Portable password + optional Windows quick unlock](PORTABLE_TASK_ENCRYPTION_PASSWORD_DPAPI_2026-10-20.md).
> This is preserved as historical prototype documentation. No live migration
> or portable encryption is enabled by either document.

> **Development-stage codec only. No task files are encrypted or migrated by this commit.**
> Release remains disabled and the Firefox malware warning remains unresolved.

## What the application currently stores

`ListOfJobOptions::PersistToUserData` serializes every `JobOptions` record through
`QDataStream::Qt_5_2` to `tasks.bin`. Version 3 records include source/destination
paths, arbitrary `extra` rclone arguments, metadata and UUIDs. Some fields can
contain access tokens or passwords. `QSaveFile` currently prevents certain
partial overwrites but does not encrypt those contents.

## Proposed protected file format

- The inner payload **remains byte-for-byte the existing Qt 5.2 QDataStream**.
  Do not change `JobOptions::classVersion`.
- Outer data starts with an eight-byte marker `RBTSENC1` followed by a
  Windows DPAPI authenticated, current-user-protected ciphertext.
  Future `RBTSENC*` versions must be treated as protected/unsupported,
  **never accidentally parsed as legacy plaintext**.
- The DPAPI plaintext includes a fixed inner domain marker
  `RBTaskPayload/1:` before serialized payload bytes. The application uses
  additional non-secret DPAPI entropy specific to this file format.
- DPAPI uses current-user scope without `CRYPTPROTECT_LOCAL_MACHINE`, and
  suppresses UI prompts. **No silent plaintext fallback** on encryption or
  decryption failure. Errors must not reveal task content.
- Keep encrypted bytes in the same transactional `QSaveFile` mechanism.
  The first integration must load/verify the complete task list into a temporary
  collection **before** replacing in-memory tasks or permitting saves.
  A missing, corrupt or un-decryptable protected file must force
  **read-only recovery mode**: never turn an unreadable file into an empty list
  and overwrite it at the next save.

## Migration and recovery gates (NOT implemented yet)

1. Test codec and envelope detection using synthetic data on GitHub-hosted Windows
   and Linux CI, including damaged ciphertext, empty payloads, and unknown
   protected versions. A Linux host must explicitly refuse DPAPI encryption.
2. Implement a transactional reader with complete-list validation and no
   partial restores. Distinguish absent file from inaccessible/corrupted file.
3. Select protection by platform **and mode**: normal Windows user data may use
   DPAPI; **portable Windows and Linux must not use machine/user-bound DPAPI**
   as a transparent default.
4. Design an explicit migration with user-visible confirmation and recovery
   instructions. Test old plaintext task lists, new protected lists, failure
   and rollback, changed Windows user/device, empty lists, and secure handling
   of plaintext backups. **No automatic destructive migration**.
5. Perform interactive Windows GUI tests with fabricated tasks. Add user-friendly
   read-only recovery warnings before enabling default encryption.

## What this does and does not protect

DPAPI helps guard a saved task file at rest against another Windows account
or offline casual access when its user's Windows protection keys are not
available. It does not protect against malware running as the same Windows
user, memory inspection, privileged admins, plaintext legacy backups,
user-controlled command-line arguments or log leaks.

For portable/multi-OS portability, use a separate vetted password-based
authenticated-encryption design (e.g. audited libsodium with Argon2id
key derivation), including recovery when the passphrase is lost. Do not
store an encryption key next to the encrypted portable task file and call
that protection.

## CI outcome — 2026-10-17

[GitHub Actions run #39015169327](https://github.com/mctransik/RcloneBrowser/actions/runs/39015169327) on development commit `f77c8f6bbff09089bd824b61503ae59d535f1298` **succeeded**. Windows and Linux each passed **all 9/9 CTest suites**, including new `task_protection` alongside earlier task, proxy and credential tests. The Windows DPAPI codec compiles and passes the synthetic encryption/decryption and tamper tests; Linux compiles it and passes its platform-unavailable test path. The tests do not exercise actual application migration or an interactive GUI.

The current Windows ClamAV job scanned **70 files, 0 detections** and the historical job scanned **408 files, 0 detections**, using **3,628,169** known signatures. Windows Authenticode inventory reported **26 EXE/DLL: 24 valid, 2 unsigned, 0 abnormal**. Exact scanned Windows ZIP SHA-256: `d53dcd1524afb44420113e6d49e79dfdea3ea44e7b56a7e5635340c2335a605d`; [GitHub provenance attestation #56912537](https://github.com/mctransik/RcloneBrowser/attestations/56912537) was independently verified by CI. Public release job **skipped**.

**Not a general security clearance:** Existing `tasks.bin` remains unencrypted, no migration occurs, portable/password-based encryption and fail-closed recovery are not implemented, and previous Firefox malware warning remains unresolved. Avoid overriding browser/antivirus warnings to run the developer build.
