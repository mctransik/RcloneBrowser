# Windows handle-relative restore: API decision record (research only)

Date: 2026-10-27. **DRAFT: design investigation, not implementation or approval.**
Applies to PR #28, stacked on PR #27. All runtime, installers and actual
saved-task files remain unchanged.

## Candidate access approach

A plausible research candidate is to obtain a handle for the user-selected
root directory, then open **one child component at a time relative to that
already-open directory handle**. The Windows native `NtCreateFile` documentation
describes `OBJECT_ATTRIBUTES.RootDirectory` with a relative `ObjectName`.
`OBJECT_ATTRIBUTES` also documents `OBJ_DONT_REPARSE`. These are starting
points for a Windows-only disposable test, **not proof of secure behavior**.
Use an explicit no-reparse policy and inspect attributes and identity of
each returned handle. Keep trusted ancestor handles alive. No multi-component
path from untrusted metadata may be handed to an absolute-path reopen.

Before code is reviewed as viable, confirm user-mode SDK declarations,
linking/runtime resolution, ABI/support policy on supported Windows versions,
the difference between object-manager and filesystem reparse behavior,
handling of relative single-component names, required handle access rights
and share flags, and whether any fallback loses the security invariant.
No insecure fallback is permitted.

### Why the existing test is insufficient

`CreateFileW` with `FILE_FLAG_OPEN_REPARSE_POINT` makes it possible to
inspect the final object without following its reparse point, but it does
not establish a race-free inspection of every parent. PR #27 correctly
demonstrates identity of **opened** objects and a real NTFS junction, but
does not protect a restore across simultaneous ancestor rename/swap.

## Publishing a restored file

Research `SetFileInformationByHandle` with `FileRenameInfoEx` and a
directory `RootDirectory` handle for a *single validated relative final
component*. Never set replace-if-exists or POSIX replacement semantics
for the no-overwrite case. Confirm the filesystem behavior under concurrent
creators and case-insensitive collisions instead of assuming atomicity
or durability. Stage a synthetic file under the intended verified directory
on the same volume and retain its opened handle during tests.

Reopening by a pathname for commit invalidates the claimed protection.
Investigate data and directory flush behavior, recoverability after process
termination or system power loss, and cleanup of incomplete staging.
Do not claim a full crash-safe transaction merely from a successful rename.

## Mandatory Windows fixture gates (future code)

1. All activity under `QTemporaryDir`; no actual `tasks.bin`, user
   documents, account credentials or cloud endpoints.
2. Ordinary nested directory with one-component handle-relative opens;
   verify identities, volume and expected bytes.
3. Genuine disposable NTFS junction in an intermediate component: deny,
   do not write to the junction target; treat inability to exercise
   this mandatory case as a CI failure.
4. Rename/replace intermediate path *after* a parent handle has been
   obtained: verify any subsequent child opening stays rooted in the
   pinned object or fails; never writes into the replacement directory.
5. No-overwrite collision including case variants; rejected attempts
   preserve the old bytes.
6. Explicitly reject parent traversal, absolute/drive-relative paths,
   device names, alternate streams, invalid Unicode/name encodings,
   and unexpected separators *before any mutation*.
7. Inject failures in create/write/flush/rename/cleanup; verify
   old destination bytes unchanged, and distinguish cleanup failure
   from successful restore.
8. Unsupported filesystem, privileges, file-share or API semantics:
   fail closed and report limitation; no silent pathname fallback.

## Boundary and decision gate

This prototype cannot secure against administrators/kernel compromise,
malicious file-system drivers, or all same-user interference without a
documented threat-model decision. Hardlinks, network shares, ReFS,
ACL inheritance, rename semantics, concurrent process permissions, and
reboot durability require separate assessment. An independently reviewed
implementation design and passing targeted native tests are needed
before production integration.

Sources (Microsoft):
- NtCreateFile: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntcreatefile
- OBJECT_ATTRIBUTES: https://learn.microsoft.com/en-us/windows/win32/api/ntdef/ns-ntdef-_object_attributes
- FILE_RENAME_INFO: https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info
- FileRenameInformationEx: https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-fscc/4217551b-d2c0-42cb-9dc1-69a716cf6d0c
- Reparse operations: https://learn.microsoft.com/en-us/windows/win32/fileio/reparse-point-operations
