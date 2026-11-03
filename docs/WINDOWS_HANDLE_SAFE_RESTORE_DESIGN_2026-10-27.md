# Windows handle-safe restore: research gate (2026-10-27)

> **DESIGN-ONLY DRAFT. NOT A SECURITY FIX.** No production implementation,
> no user files, no passwords, no release and no merge approval.
> Stacked on PR #27 (Windows native reparse/identity test-only research).

## Objective and assets

A portable encrypted backup may be restored into an explicitly selected
directory on Windows. The untrusted inputs are the backup's serialized metadata,
all relative entry names, and the contents of any destination directory that an
untrusted local process can modify. Protect existing destination data and avoid
writing outside the explicitly approved restore root.

Existing PR #27 demonstrates actual NTFS junction detection and opened-handle
identity checks. It **does not** safely resolve a multi-component pathname or
protect against parent-directory replacement after checking it.

## Required invariants (not yet established)

1. **Root binding.** The operator explicitly chooses a destination. Before any
   writes, obtain an operating-system handle to that directory and establish
   its identity/volume. Do not silently switch to a path that later resolves
   to a different directory.
2. **Traversal by trusted handles.** For each path component, do not perform
   check-by-name followed by reopen-by-absolute-name. Resolve/create children
   relative to already validated directory handles with appropriate Windows
   semantics; examine the resulting opened object rather than cached metadata.
   Availability and documented security properties of the required native
   APIs must be reviewed separately.
3. **Reparse fail closed.** Reject reparse points at every traversed component,
   including unknown tags; never assume FILE_FLAG_OPEN_REPARSE_POINT on the
   *last* component automatically secures ancestors. Decide explicitly how
   hardlinks, UNC/SMB, alternate data streams, device paths and volume changes
   are handled; default to unsupported until assessed.
4. **Untrusted names.** Reject absolute paths, parent traversal, drive-relative
   entries, empty/special components, reserved Windows device names, trailing
   dot/space normalization collisions and alternate-stream syntax. Define and
   test duplicate/case-folding behavior; do not silently normalize two
   entries onto one destination object.
5. **Authenticated content first.** Backup metadata, requested paths and file
   contents must be authenticated under the portable envelope before they
   authorize filesystem mutations. Reject invalid authentication and
   unsupported metadata without creating/altering destination entries.
6. **No overwrite by default.** Stage a new file safely on the intended volume,
   flush and commit through a verified destination directory context with
   explicit collision policy. Preserve a pre-existing file on conflicts,
   interruption or unexpected identity changes. Directory creation also
   requires rollback/cleanup semantics.
7. **Failure is non-destructive.** Deny on ambiguous filesystem behavior or
   failed OS calls. Keep recoverable state explicit. Avoid reporting successful
   restore until durable commit/verification succeeds.

## Proposed design review before coding

- Compare documented Win32 capabilities with documented native NT
  handle-relative operations (OBJECT_ATTRIBUTES RootDirectory and
  NtCreateFile); review ABI support, API stability and maintainability.
  **Do not** treat a name-based Win32 create/reopen sequence as automatically
  TOCTOU-safe.
- Establish the precise security boundary against same-user processes,
  administrator/kernel attackers, filesystem filter drivers, network
  filesystems, and processes with modify permissions on ancestors.
- Determine handle permissions and share flags for opening directories and
  files without accidentally requiring administrator privileges.
- Specify the commit primitive and its guarantees for same-volume temp files,
  replacement avoidance, crashes, and power failure. Flush guarantees and
  rollback cannot simply be assumed.
- Independent Windows filesystem/security review is a prerequisite to
  claiming safety and to linking restore code into the application.

## Next isolated tests (synthetic temporary files only)

| Scenario | Expected outcome |
| --- | --- |
| Ordinary nested temporary directory | Restore stays under selected root |
| Replace an ancestor directory between check and use | No write outside pinned approved root |
| Genuine NTFS junction at a parent or final component | Deny without following it |
| Swap path entry after original handle opens | Pinned identity remains distinct |
| Existing destination filename or case-insensitive collision | No overwrite |
| Relative `..`, absolute, ADS, reserved device, malformed names | Reject before mutation |
| Invalid authenticated metadata | No filesystem mutation |
| Inject write, flush, commit or rename failure | Existing data stays intact |
| Interrupted staged restore | No partial published target |
| UNC, SMB, hardlink, cross-volume behavior | Explicitly unsupported or documented/tested |

These are **requirements**, not assertions that tests already exist or pass.
Initial harness should be a standalone Windows-only QtTest behind
`RB_BUILD_PORTABLE_CRYPTO_PROBE` without install or packaging rules;
it must use only disposable fixture bytes and fail rather than skip
a required native NTFS safety test. Linux CI stays unaffected.

## Release and review gate

Keep this PR **DRAFT** and stacked on #27. No change to app runtime or real
`tasks.bin`, no real passwords, no migration, no new release artifacts and
no claims of production protection. Before implementation: select and document
handle-relative Windows API approach, define collision/rollback semantics,
and seek independent threat-model review. All tests and CI must pass before
considering any further integration or merge.
