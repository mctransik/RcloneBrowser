# Saved task data: atomic writes and file permissions (2026-10-15)

**Status: implemented on the development branch; not yet CI-validated.**

The legacy tasks.bin task store uses a Qt QDataStream (version Qt_5_2) and includes custom rclone CLI options (JobOptions::extra). These options may contain sensitive user-supplied tokens and passwords. This is **not an encrypted file** and users should avoid putting credentials in saved extra flags.

## Confirmed original behavior

- Existing PersistToUserData() opened tasks.bin using QFile::WriteOnly, **truncating the existing task file before serialization completed**. Disk full, cancellation, and stream/flush failure could irreversibly destroy the prior saved task list.
- On Unix, ordinary file creation / retention could leave group or world permissions, depending on the user's umask or old file mode. Portable mode also stores tasks in an app-related directory. **Windows confidentiality depends on inherited NTFS ACLs**, not Qt's POSIX permission bits.

## Implemented mitigation

- Added TaskStorage::saveAtomically() using QSaveFile with direct-write fallback **disabled**. The previous tasks.bin is replaced only when the complete QDataStream writes successfully and QSaveFile::commit() succeeds; failures return false without truncating the old copy.
- Preserves the on-disk QDataStream Qt_5_2 version and all existing JobOptions fields; **no incompatible migration or task deletion**.
- On non-Windows hosts, sets owner-only read/write permission on the temporary file before writing sensitive data, including when replacing an older permissive task file. On Windows, **no claim of cryptographic encryption or ACL hardening** is made; the folder's security policy remains relevant.
- Keeps existing saved-task UI signaling after successful commit.
- Adds Qt tests with synthetic data for format compatibility, interrupted serialization preserving the previous file, rejection of invalid output destinations, and POSIX permission tightening.

## Boundaries and follow-ups

- This fix **does not encrypt** tasks.bin or remove passwords already included in user-provided rclone options.
- Atomic commit cannot prevent all corruption scenarios (storage hardware faults, compromised filesystem, etc.). Special filesystems may refuse atomic replacement and in that case **saving fails instead of writing in place**.
- Existing application callers of Persist/Forget do not consistently surface a save failure in the user interface; add visible failure feedback in a follow-up once the operation/result contracts are reviewed.
- Proxy credential storage, command-line argument exposure and log redaction remain separate open reviews. Avoid testing with real production cloud credentials while security assessment is incomplete.

Reference: [Qt QSaveFile class](https://doc.qt.io/qt-6/qsavefile.html).
