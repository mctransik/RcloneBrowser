# Saved task restore: fail-closed staging (2026-10-19)

> **Prototype / draft pull request. CI and interactive GUI validation pending.**
> This change does NOT switch on DPAPI encryption, migrate task files, or authorize a public release.

## Problem being addressed

Previously, `ListOfJobOptions::RestoreFromUserData` appended decoded items directly into the live list as it read the file. An exception could leave a partially restored in-memory list. The singleton ignored the restore failure and later Save/Delete operations could replace an existing unreadable file with the partial/empty list.

## Scoped change

- `TaskStorage::readTaskBytes` distinguishes absent files from unreadable existing files, rejects symlinked/non-regular files and inputs above a 64 MiB safety bound. It does not rewrite the file.
- When the file is genuinely absent, the application may start with a new empty task list. A present empty file is also a valid empty legacy list.
- For a present file, the app checks `TaskProtection::inspectFormat` **before** parsing. Current/unknown protected DPAPI envelopes are deliberately **not decrypted yet**. They enter read-only recovery until decryption and migration have been fully designed, implemented and tested.
- Existing legacy QDataStream records are decoded into a temporary collection. Only when every record and stream status is valid are they swapped into the live task list.
- On any read, unsupported-format, or decode error, temporary entries are destroyed, existing files are left untouched, and a session-long read-only recovery flag blocks Persist, Forget, and the underlying save method.
- The GUI schedules an explicit generic recovery warning with no task content, secret values or local file paths. Recovery requires repairing the file or restoring a trusted backup and restarting; there is no silent reset.
- The on-disk legacy stream format and `JobOptions::classVersion` are unchanged.

## Checks and open gates

Synthetic Qt tests cover absent/present empty, byte-preserving legacy reads, directory, symlink (Unix), and excessive file size, along with the existing envelope-identification tests. **CI result pending** for this revision. These unit tests do not constitute end-to-end interactive task-loading tests.

Before enabling DPAPI by default or allowing a migration:

1. Verify the entire save/load lifecycle with malformed and fully valid `JobOptions` records in dedicated tests, including deletion rollback and restarting after recovery.
2. Evaluate symlink/portable storage compatibility and explicit user recovery affordances.
3. Add protected-file transactional decryption and user-confirmed migration with rollback/backup. No automatic plaintext fallback.
4. Validate Windows and Linux builds and manually verify GUI behavior using only synthetic files.
5. Complete all independent security, malware-warning, licensing and release checks.

No release should be published based on this task-file change alone.
