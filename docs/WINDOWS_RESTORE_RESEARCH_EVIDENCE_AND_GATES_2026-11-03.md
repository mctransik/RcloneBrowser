# Windows restore research: evidence, limitations and release gates (2026-11-03)

> **RESEARCH AUDIT — NOT A PRODUCTION SECURITY APPROVAL.**
> Draft PR #28 (stacked on #27). No production restore implementation, real backups, user files, passwords, application linkage, release or merge approval.

## Decision

**CONTINUE RESEARCH. BLOCK PRODUCTION INTEGRATION AND RELEASE.**

The isolated Windows fixtures have passed, but passing a synthetic test does not establish an end-to-end, race-free Windows restore. The evidence supports specific native API behaviors only.

Reviewed source commit: e7df4888fbbfb87dfe2c11780ec908e06cc0c32d.
Last verified workflow: https://github.com/mctransik/RcloneBrowser/actions/runs/41669979630

- Windows: **19/19 CTest suites passed**.
- Linux: **16/16 CTest suites passed**.
- Windows ClamAV job succeeded. A scan is not a malware-free certification.
- Release and attestation jobs were **skipped**. Skipped jobs are not security approvals.

## Evidence matrix

| Area | Observed in isolated research | NOT yet established |
| --- | --- | --- |
| Opened-object identity | A pinned directory/file handle retains the original file identity after a disposable pathname changes. | A secure, full restore lifecycle; hostile same-user modifications, administrator/kernel compromises, unexpected filesystem behavior. |
| Native relative traversal | Single-component NtCreateFile opens relative to a pinned RootDirectory and returns the expected original fixture bytes after path replacement. | Secure resolution and creation of arbitrary multi-component names from authenticated backup metadata. |
| NTFS junctions | Real disposable junctions are inspected without following them, rejected by the conservative reparse rule; target sentinels remain unchanged. | All reparse tags/types, hardlinks, cloud placeholders, alternate data streams, network/UNC or all possible racing interleavings. |
| Concurrent namespace mutation | Controlled check/swap/use and six synchronized two-thread junction cycles were detected at the intended checkpoints. | An uncontrolled adversary racing the actual create/write/commit operation. Retrying path checks does not close a TOCTOU window. |
| Missing input and cancellation | A missing native leaf fails; a later valid open works; QSaveFile cancellation does not publish incomplete synthetic data. | Full cleanup, rollback, process-kill, disk-full or power-loss guarantees. |
| Benign staging/commit | A known public payload digest is checked; published bytes are re-read and hashed after a nonhostile QSaveFile commit. | Authenticated encrypted backup-to-filesystem binding; secure path-based commit against interference; verified disk durability. |
| Stale destination identity | A deliberate pathname substitution before a proposed QSaveFile commit is observed and causes the test to cancel staging. | **Atomic** identity verification plus publication; mutation remains possible after the check and before a later path-based commit. |
| Pinned-handle no-overwrite rename | NtSetInformationFile using native FileRenameInformation (class 10) publishes to the original pinned directory despite a decoy at the old path. An occupied target name remains intact. | Atomic replacement of existing files, complete staging safety, consistency on other Windows versions/filesystems or hostile concurrent writers. |
| Owner/DACL snapshots | GetSecurityInfo reads owner and DACL through the opened source file handle before and after the test rename. Observed snapshots agree. | Whether the ACL is **safe**, restricts plaintext access appropriately, follows correct inheritance policy, or resists malicious ACL changes. Equality is not authorization. |
| Research isolation | CMake registers the Windows probes as standalone CTest executables under the research build option. No production application linkage/install rule was observed. | Independent security review, production readiness, signing or distribution authorization. |

Evidence files:
- tests/test_portable_windows_reparse.cpp
- tests/test_portable_windows_handle_pinning.cpp
- tests/test_portable_windows_native_relative.cpp
- src/CMakeLists.txt
- .github/workflows/update.yml

## Blocking gaps: P0 (must solve before application integration)

1. **Threat boundary and root binding.** Define who can modify the selected root and ancestors. Prove the entire restore stays bound to an approved directory handle or fails closed. Handle errors, deletion/rename and unsupported volumes.
2. **Authenticated paths and metadata.** Tie restored content, length, names and directory metadata to authenticated backup data. Explicitly reject absolute/drive-relative paths, parent traversal, reserved device names, ADS syntax, separator tricks, trailing dots/spaces, invalid Unicode, case collisions and duplicated destination entries before mutation.
3. **Handle-relative staging.** Establish and review an actual create/write/verify/flush/publish chain beneath validated handles with no-follow and no-overwrite rules. Current QSaveFile staging is pathname-based and is not certified safe against destination substitution.
4. **Hostile concurrency.** Test actual concurrent create/write/publish, not just synchronized check/swap/read. A final pathname recheck is still TOCTOU-prone. Define error and conflict behavior.
5. **Durability and rollback.** Define when a restore is complete, including flush requirements, space exhaustion, read-only/locked files, process termination, crashes/power loss, and recovery from incomplete directory trees or temporary files. No multi-file atomicity is established.

## Required analysis: P1

6. **Windows API portability.** Review native NtCreateFile/NtSetInformationFile ABI, dynamic symbol use, structure layouts, permissions, NTSTATUS errors, Windows versions and filesystem capabilities. Never introduce a silent insecure absolute-path fallback.
7. **Access control.** Establish a positive authorization policy for owner, DACL, inheritance, staged plaintext visibility, destination directory permissions and unprivileged/same-user interference. Testing unchanged owner/DACL does not prove secure ACLs.
8. **Unsupported storage.** Treat cross-volume rename, UNC/SMB, ReFS, hardlinks, other reparse types, filesystem filters and odd names as denied until tested and supported explicitly.
9. **Independent review.** Obtain independent Windows filesystem and security review before any real backup restore or application linkage.

## Recorded negative findings (retain for traceability)

- Commit 16f7a94: Win32 SetFileInformationByHandle(FileRenameInfo) with a non-null target RootDirectory failed on the tested CI runner with Win32 **87**. Do not generalize that behavior to every supported Windows platform.
- Commit 1c4c89b: native NtSetInformationFile accepted the relative rename, but Qt could not re-open the published file while the source handle still had DELETE access.
- Commit e7df488: closing the source DELETE handles **after** identity/ACL observations but **before** separate Qt file reads resolved the test failure. This preserves the experimental invariant while testing only that scenario.
- All actual test data is synthetic and disposable. A green workflow does not validate a real cryptographic restore pipeline.

## Recommended work order

1. Write a design-only, handle-relative create/stage/verify/rename **state machine** with permissions, explicit no-overwrite policy, failure states and secure cleanup. Review it before coding a product feature.
2. Add isolated ACL allow/deny and inheritance tests, including a restrictive staged-plaintext policy. Add case-insensitive collisions and malformed-name negative fixtures.
3. Inject failures at create/write/verify/flush/commit/cleanup boundaries, followed by explicit hostile-concurrency and separate power-loss tests.
4. Independently review the resulting design/implementation. Keep PR #28 DRAFT and stacked. No release, merge, user data or real backup restore until the blocking P0/P1 gates are satisfied.

## Related design/reference documents

- docs/WINDOWS_HANDLE_SAFE_RESTORE_DESIGN_2026-10-27.md
- docs/WINDOWS_HANDLE_RELATIVE_API_RESEARCH_2026-10-27.md
- docs/PORTABLE_RESTORE_PATH_SAFETY_RESEARCH_2026-10-27.md
- docs/PORTABLE_ATOMIC_RESTORE_RESEARCH_2026-10-27.md
- docs/PORTABLE_WINDOWS_REPARSE_RESEARCH_2026-10-27.md
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_file_rename_information
- https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info
- https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-getsecurityinfo

**Status remains DESIGN/TEST RESEARCH ONLY. No production security certification.**
