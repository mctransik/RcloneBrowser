# Secure Windows restore: handle-bound transaction state machine (design draft, 2026-11-09)

> **DESIGN ONLY — NOT AN IMPLEMENTATION OR SECURITY FIX.**
> Research for draft PR #28, stacked on #27. Do not merge, ship or use with
> real backups. This document defines a candidate sequence and explicit
> stop conditions; no runtime changes or new test coverage are implied.

## Purpose and explicit boundary

Restore a **new** file from *already authenticated* backup content into an
operator-approved Windows directory, without redirecting the write outside
that directory or overwriting an existing entry.

The initial candidate supports **no-overwrite, one file per transaction**.
Restoring an entire directory tree atomically, replacing an existing file,
preserving arbitrary original ACLs, and rollback of a committed publication
are **out of scope**. A user can modify the approved root concurrently;
same-user processes with equivalent access may also interfere. This design
cannot defend against an administrator, kernel actor, or hostile filesystem
driver. Whether a same-user writable root can ever meet the desired
confidentiality and integrity guarantees is a blocking design question, not
something a directory handle alone resolves.

Initially restrict testing to locally verified NTFS within a disposable
QTemporaryDir. Treat UNC/SMB, ReFS, cross-volume operations, hardlinks,
unknown reparse behavior, and unsupported Windows versions as **DENIED** until
the API contract and threat boundary are reviewed.

## Hard invariants

- **I1 — No mutation before authentication:** decrypt/authenticate the
  encrypted envelope and all serialized metadata, including *full* paths,
  expected contents/digests, sizes and entry types. Reject unsupported data
  and duplicate/case-colliding paths before opening or creating output.
  The tested synthetic envelopes do not yet authorize filesystem operations.
- **I2 — Selected root stays pinned:** obtain an approved root-directory
  handle, inspect identity/volume and reparse attributes on the opened
  object. Every child is reached through a single validated component
  relative to a verified parent handle. An absolute-path reopen is never a
  fallback. A renamed root must not redirect subsequent operations.
- **I3 — Reject unsafe names and objects:** before any mutation, reject empty,
  dot and parent components, separators within a component, absolute or
  drive-relative syntax, ADS colons, Windows device names, trailing dot/space,
  invalid Unicode, and normalization/case collisions. At every hop reject
  any reparse attribute, even an unknown tag. Define an explicit rule for
  final hardlink count; do not assume link safety merely from no-reparse.
- **I4 — Staging cannot be guessed into another destination:** create a fresh
  one-file stage beneath the same **pinned** target-parent handle, using
  create-new/no-overwrite semantics. Do not use a path-based Qt staging API
  as the security boundary. Hold the opened staging handle and validate its
  identity, reparse attributes, expected volume and owner/DACL.
- **I5 — Restrictive plaintext access:** specify a documented security
  descriptor/owner/inheritance policy for the stage *before creation*.
  Merely observing the same DACL before/after rename is insufficient.
  Deny or halt when the required isolation cannot be enforced. A hostile
  process with the same token or privileged access can defeat some ACL
  assumptions; threat-model it explicitly.
- **I6 — Same handle and verified bytes:** write via the already-open stage
  handle, check exact byte counts and errors, flush it, and verify bytes and
  authenticated digest from that same object (or an independently validated
  same-identity handle). Never use a name-based reopen as an authorization
  step. Define and test memory cleanup for plaintext separately.
- **I7 — Commit is relative and non-replacing:** while the target parent and
  stage remain pinned, publish a validated single-component leaf using a
  reviewed handle-relative native rename with replacement **disabled**.
  If the target exists, including a case-insensitive collision, abort and
  preserve it. Verify target identity/contents as far as supported; never
  silently retry by pathname.
- **I8 — Fail closed with honest durability claims:** any unknown NTSTATUS,
  failed ACL check, flush, file identity mismatch, unsupported filesystem,
  collision or cleanup failure is a reported failure or unresolved result.
  A successful rename is not proof of persistence after power loss.

## Candidate transition map

| State | Required action / check | Success transition | Failure transition |
| --- | --- | --- | --- |
| S0: Untrusted | Authenticate full envelope, contents, metadata and relative entry names in memory. No output mutation. | S1 | F0: rejected, no output created |
| S1: Authorized | Operator explicitly approves an existing root; inspect pinned root handle, volume, reparse/ownership and filesystem support. | S2 | F0 |
| S2: Resolved | Walk each component *relative to pinned parent*, validate opened directory and attributes, define if new subdirectories are allowed. **Initial candidate: pre-existing directories only.** | S3 | F0 |
| S3: Staged | With parent pinned, create a new temporary leaf via handle-relative create-new. Apply/check restrictive stage ACL and verify volume/identity. | S4 | F1: cleanup staged leaf through controlled handles |
| S4: Written | Write authenticated bytes through stage handle, check lengths and errors, flush, read back/verify identity and digest. | S5 | F1 |
| S5: Ready | Revalidate handle-bound stage and target parent, confirm no known disqualifiers; do not depend on a separate pathname check. | S6 | F1 |
| S6: Publishing | Perform **no-replace** handle-relative rename to final leaf, retaining required handles until outcome is known. | S7 on confirmed success | F2 if collision/failure; determine whether stage still exists |
| S7: Published | Confirm file identity, expected bytes and permitted owner/DACL; define completion durability policy. | S8 only if every claimed postcondition holds | F3: published but verification/durability unresolved; never claim rolled back |
| S8: Complete | Report success for **one file**, under explicitly stated persistence assumptions. | Terminal | — |
| F0: Rejected | Close opened handles, no staged/published data. | Terminal failure | — |
| F1: Precommit failure | Attempt *handle-bound* disposal of staged object; preserve existing files. Verify cleanup when feasible. | Terminal failure if cleanup confirmed | F4 if cleanup fails |
| F2: Commit not confirmed | Distinguish file-already-exists, access denied, stage still present, and ambiguous status; never overwrite or auto-retry by pathname. | Terminal conflict/failure after verified cleanup | F3/F4 when ownership or cleanup cannot be determined |
| F3: Published state uncertain | **Do not** delete/overwrite an existing final name blindly. Record explicit partial/uncertain state for human-reviewed recovery. | Terminal unresolved | — |
| F4: Cleanup failure | Record an orphaned stage or ambiguous object identity; do not claim safe rollback. Recovery procedure required before use. | Terminal unresolved | — |

The states describe an **intended protocol**, not proven atomic operations.
Revalidating a path between S5 and S6 would not close TOCTOU; all names used
for mutation must be bound to already-verified directory/file handles.

## Windows API decisions still requiring independent review

- Native one-component opens: NtCreateFile with RootDirectory is a tested
  *candidate*. Review no-follow semantics, file/share flags, creation
  dispositions, the object manager versus filesystem reparse boundary,
  status handling, Windows/SDK compatibility and privileges.
- Native rename: the isolated fixture used NtSetInformationFile with
  FileRenameInformation (native class 10), a pinned destination directory
  and replacement disabled. Win32 SetFileInformationByHandle(FileRenameInfo)
  returned error 87 in earlier CI: do not treat this as universal evidence.
- Creating a file beneath a parent handle, secure deletion of an orphan
  stage, and verified flush behavior are **not implemented or tested**.
  Explicitly resolve whether a directory metadata flush is supported and
  what durability can actually be promised on each supported filesystem.
- If an API is absent, denied, or has ambiguous semantics, **DENY**; no
  CreateFileW absolute-path fallback or Qt rename fallback.
- ACLs: propose the minimal required rights (including READ_CONTROL,
  FILE_ADD_FILE, DELETE, SYNCHRONIZE) for each *operation*, then verify
  inherited DACL policy and any security descriptor used at stage creation.
  A successful owner/DACL equality check is not an access-control policy.
- Final name collision behavior, case folding and object replacement must
  be tested with a second local process at publish time. This initial
  candidate deliberately prohibits replacement of existing targets.

## Next isolated, disposable tests

1. **Handle-relative stage creation:** parent renamed and old pathname used
   as a decoy; new stage must appear under the pinned parent or fail closed.
2. **Stage permissions:** verify actual owner/DACL against policy; exercise
   allow/deny and inheritance cases on supported Windows runners.
3. **Collision and name handling:** create case-colliding targets and reject
   all problematic relative names *before any filesystem mutation*.
4. **Injected failures:** deny or inject errors at creation, ACL inspection,
   write, flush, readback, rename and cleanup. Verify neither pre-existing
   data nor outside sentinels are modified, and report orphan stages.
5. **Concurrent process:** race child creation/publication and parent
   namespace changes; verify no write escapes the approved root. Test only
   with public bytes in QTemporaryDir.
6. **Durability separate from unit tests:** process termination and actual
   power-loss behavior need their own environment-specific review; do not
   claim them from a green CI run.

## Gate and references

This state machine needs **independent review** against the threat model
before any implementation linked to RcloneBrowser. Maintain PR #28 as
DRAFT, keep it stacked on #27, keep releases disabled, and avoid any real
tasks.bin, backups, cloud files, passwords or personal files.

Related research:
- docs/WINDOWS_RESTORE_RESEARCH_EVIDENCE_AND_GATES_2026-11-03.md
- docs/WINDOWS_HANDLE_SAFE_RESTORE_DESIGN_2026-10-27.md
- docs/WINDOWS_HANDLE_RELATIVE_API_RESEARCH_2026-10-27.md

Primary Microsoft documentation to verify during review:
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntcreatefile
- https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-ntsetinformationfile
- https://learn.microsoft.com/en-us/windows/win32/api/aclapi/nf-aclapi-getsecurityinfo

**No integration or security approval is asserted by this design document.**
