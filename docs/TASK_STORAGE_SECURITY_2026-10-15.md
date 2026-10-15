# Saved task data: atomic writes and file permissions (2026-10-15)

**Status: verified by Windows and Linux CI on 2026-10-15; still not a malware-free or general security certification.**

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

## CI validation — 2026-10-15

[GitHub Actions run #38755582451](https://github.com/mctransik/RcloneBrowser/actions/runs/38755582451) on development commit `317e0d0e00ce1ff6ba87deeaacc78071064fd229` **passed**. CTest completed **4/4 test suites on Windows** and **4/4 on Linux**, including `task_storage`. The `task_storage` Qt tests exercise serialization compatibility, preservation of the old file on a failed serialization, invalid save destinations, and Unix permission restriction (the latter runs only on Unix). This is simulated regression coverage, not a power-loss hardware test.

The same run reported **24 valid and 2 unsigned** Windows executable/DLL Authenticode statuses (no abnormal statuses); ClamAV scanned **70 files with 0 detections** in the current Windows payload and **408 with 0 detections** in six historical Windows packages. The independently checked GitHub provenance attestation was created for the **exact scanned** Windows application ZIP, digest `f7ad02f528ee5917992e194f949c3665a9b3f2812da98287e50a2e9055696414`. The `release` job remained **disabled/skipped**.

These findings validate the focused task-storage change in the CI environments, not confidentiality of `tasks.bin` as a whole. Plaintext sensitive flags, Windows NTFS ACLs, app-level failure notifications, and the historical Firefox malware warning remain open issues.

## Visible write errors and transactional task list (implemented; CI pending)

The original saved-task GUI closed the transfer dialog after calling `Persist` even when `QSaveFile` reported failure. Deleting a task also removed it from the in-memory list before a disk commit, without restoring it on error. Edited task fields were mutated on the shared `JobOptions` instance before a failed save.

The focused update now:

- Gives `ListOfJobOptions::Persist` and `Forget` an explicit **success/failure** result rather than the former "is new"/"was known" meaning. The only production call sites ignored the old results.
- Rolls back newly appended task pointers and unsuccessful deletions (including their original ordering). The successful save still emits `tasksListUpdated`; failure does not.
- On an unsuccessful edited-task save, restores the original `JobOptions` fields without destroying the user's unsaved GUI input. The edit dialog remains open for a retry.
- Shows clear critical dialogs when saving or deleting fails, rather than falsely behaving as though the operation succeeded. Error text deliberately does not include task names, command arguments, secrets, or untrusted filesystem paths.
- Adds a Qt test suite for successful and failed append/delete transactions, list ordering, repeated task save and invalid/null operations, with synthetic data only.

**Limits:** These tests exercise in-memory transactional behavior and failed-write simulation, not the complete interactive Qt dialog; manual GUI checking with disposable tasks remains necessary. The prior task-storage tests still exercise actual `QSaveFile` behavior. No encryption is added, and nothing here resolves the historical Firefox malware alert. Release remains disabled.
