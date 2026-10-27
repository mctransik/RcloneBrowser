# Windows-native NTFS junction and open-handle identity research (2026-10-27)

**DRAFT TEST-ONLY RESEARCH.** No changes to RcloneBrowser runtime,
`tasks.bin`, real passwords, encrypted backups, releases or production
access control. This small experiment continues PR #26.

## What PR #26 could not test on Windows

Qt's `QFile::link()` is not a substitute for a Windows kernel-level
NTFS junction or file symbolic link. A Windows `.lnk` shortcut does
not establish the same risk as an NTFS directory reparse point.
`QFileInfo` pathname checks are also insufficient to defend against
another process changing a directory entry between verification and use.

Microsoft Windows documents `FILE_ATTRIBUTE_REPARSE_POINT`, native
`FileAttributeTagInfo`, `FILE_FLAG_OPEN_REPARSE_POINT`, and reparse
control operations (`FSCTL_SET_REPARSE_POINT`,
`FSCTL_DELETE_REPARSE_POINT`). Reparse-point information belongs
to the **opened object**, not just a lexically normalized pathname.

## Scope of new Windows-only CTest fixture

New target: `tests/test_portable_windows_reparse.cpp`, registered
as `portable_windows_reparse` only on Windows when
`RB_BUILD_PORTABLE_CRYPTO_PROBE` is enabled.

The test uses only the Windows API, Qt test infrastructure, and tiny
**public synthetic files in QTemporaryDir**. It does not use any
password, cryptographic key, real task, network service or user file.

### Deterministic tests

1. Open ordinary disposable file/directory handles with
   `CreateFileW(FILE_FLAG_OPEN_REPARSE_POINT |`
   `FILE_FLAG_BACKUP_SEMANTICS)`. Collect `FileAttributeTagInfo`
   and `BY_HANDLE_FILE_INFORMATION`; verify normal objects are
   not reparse points and accessible parents are ordinary directories.
2. A pure conservative rule rejects any object with
   `FILE_ATTRIBUTE_REPARSE_POINT`, including mount points,
   symbolic links, unknown/other tags, and even tag=0 if the
   attribute is present.
3. Create an **actual disposable NTFS junction** using the
   documented `FSCTL_SET_REPARSE_POINT` structure; inspect the
   link **without following it**; assert mount-point tag and
   fail-closed parent rejection. Delete its reparse data with
   `FSCTL_DELETE_REPARSE_POINT` and verify original public
   target-file bytes remain unchanged. This test is **mandatory**;
   missing privileges or unsupported filesystem make the Windows
   test fail rather than silently claiming a passed junction test.
4. Keep an existing file handle open, rename its pathname, then
   create another public file at the old pathname. Compare
   volume serial and file index values: the old **open handle**
   retains its original identity even though a fresh name lookup
   identifies a **different file**. This demonstrates why a
   preflight check followed by a path-based open can be unsafe.

Expected CI top-level CTest suites: **Windows 17 / Linux 16**.
The new Windows test does **not** run on Linux; the existing Linux
POSIX symlink test remains mandatory. Release stays disabled.

## Security limits and unimplemented product work

- This fixture is **NOT** a complete secure restore API. Opening
  the LAST path component with `FILE_FLAG_OPEN_REPARSE_POINT`
  does not protect against reparse traversal in ancestor components
  or malicious path replacement during repeated name lookups.
  Scanning parents with ordinary path APIs is susceptible to TOCTOU.
- `GetFileInformationByHandleEx` and file IDs can demonstrate
  object-identity changes and help design validation. They are not
  by themselves an authorization policy or defense against
  file-ID reuse after a handle is closed.
- Native NTFS junction behavior does not cover all Windows
  symlink types, third-party reparse tags, cloud placeholders,
  SMB/UNC redirections, alternate data streams, hardlinks,
  network shares, ACL mistakes, final-object permissions,
  malicious race scheduling or directory-handle-relative writes.
- A future implementation needs reviewed handle-relative traversal,
  no-follow/open flags, file/volume identity checks,
  destination directory ownership and ACL controls,
  atomically staged commit semantics and resilience against
  replacement under concurrency.
- The fixture does not link to cryptographic recovery or read
  `RBTSENC2`. It does not upgrade the draft file format or
  approve memory locking, password normalization or real-data use.
- Windows test host capabilities can differ from consumer machines.
  Successful CI only supports behavior on its particular runner,
  and a failing junction-creation permission is a test-environment
  limitation to be resolved without weakening security assertions.

References:
- https://learn.microsoft.com/en-us/windows/win32/api/winioctl/ni-winioctl-fsctl_set_reparse_point
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/ns-ntifs-_reparse_data_buffer

**Leave PR DRAFT and unmerged. No release or deployment.**
