# Restore-path validation and symbolic-link risk — test-only research (2026-10-27)

**DRAFT. Not a production security boundary.** This patch modifies only
`tests/test_portable_independent_recovery.cpp`, a synthetic CTest binary
stacked on PR #25. It does not alter the application, actual backups, real
passwords, any `tasks.bin`, or a release.

## Threat model for file output

An authenticated synthetic ciphertext should not be restored over the
encrypted original, into a directory, through a symbolic-link redirection,
or to an unintended parent outside the intended temporary directory. The
earlier experiment used a path equality/canonical comparison but did not
reject all these obviously hazardous path forms.

A real attacker might change filesystem entries **between checking a
path and opening/committing it**. This time-of-check-to-time-of-use
(**TOCTOU**) problem is explicitly **NOT SOLVED** by QFileInfo or by a
second validation. No claim is made of sandbox containment under hostile
concurrent filesystem changes.

## Test-only negative filter

The experimental `researchPathsAcceptable()` routine rejects:

- Empty or dot-segment (`.` / `..`) source/destination paths.
- Source files that do not exist, are not ordinary files or are symlinks.
- Destination paths identical to the encrypted source, or existing
  destinations that are directories, symlinks or resolve to the source.
- Paths passing through **lexically enumerated existing parent
  components** that are symbolic links, missing or not directories.
- Unexpectedly deep parent chains, conservatively, rather than looping.

This early check runs **before** the simulated OS memory-lock policy
check and **before Argon2id**. A second check immediately before
`QSaveFile::open` can detect some changes but is **not race-safe**.

All accepted flows still authenticate the synthetic ciphertext first,
and use `QSaveFile` with direct-write fallback disabled.

## Additional test cases (within the existing 16th CTest suite)

1. Encrypted source as target, directory as target, missing parent,
   `..` components and empty target: **deny without Argon2id**.
2. Ordinary existing test file and a new file within an existing
   directory: remain valid candidates; no plaintext created by checks.
3. On POSIX, use temporary symlinks to an encrypted source, a redirected
   parent and a nonexistent target (dangling link). All must be
   refused before Argon2id; encrypted source stays unchanged.

The POSIX symlink test is **explicitly skipped on Windows**. Qt
`QFile::link()` can create a Windows `.lnk` shell shortcut rather than
a kernel-level symbolic link, so treating it as Windows junction /
reparse-point coverage would be a false security claim. Windows-specific
native junction, reparse-point and object-identity tests are still
**required**, as are cases covering UNC paths and case handling.

## Unsatisfied security requirements (MUST be reviewed before app integration)

- **TOCTOU is not mitigated:** revalidation by pathname cannot guarantee
  the opened filesystem object is the object checked earlier. A secure
  implementation needs platform-specific handle-based directory
  traversal/open, no-follow and appropriate identity/ownership checks.
  Linux may use `openat2` with carefully selected resolution flags
  where available; Windows needs its own native handle/reparse rules.
  These are ideas for design review, **not implemented here**.
- Hardlinks, mount points, NTFS junctions, reparse points, network mounts,
  case-insensitivity, encoding, ACLs, directory-owner changes and
  hostile file replacement remain untested or inadequately covered.
- The `encryptedBytes` input remains separate from
  `sourceEncryptedPath`: this test does not bind disk-content identity
  to the path. No real restore authorization is implemented.
- The memory-lock verdict and any degraded acknowledgement are
  **synthetic caller-supplied values**. Actual OS locking and user
  consent are NOT implemented here. No claims about zeroizing all Qt
  plaintext copies in memory are justified.
- `QSaveFile` is useful for transactional staging; these unit tests
  do not prove power-loss durability on every filesystem, complete
  non-bypassability, or secure destination permissions.
- Further validation needs reviewed API design, actual filesystem
  fault tests, native Windows/Linux symlink/reparse tests, independent
  security review, and real-device recovery before production release.

These additional assertions are inside the existing
`portable_independent_recovery` suite; expected CI top-level suites:
**16 Windows / 16 Linux**. CI must keep release jobs disabled.

**Leave this PR DRAFT and unmerged; no user data or production behavior
may change as a result of these tests.**
