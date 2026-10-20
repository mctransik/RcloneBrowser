# Saved-task restore hardening and regression coverage — 2026-10-20

**Development-only stacked change. CI, binary and interactive GUI validation are pending.**
This branch builds on `security/task-restore-fail-closed-2026-10-19` (draft PR #7).
No new release, encryption default, automatic migration or changes to existing task files.

## Threat model and fix

The earlier `TaskStorage::readTaskBytes` checked the file size using
`QFileInfo`, but subsequently called `QFile::readAll()`. A file that grows
after the initial size check could cause an unexpectedly large read.
The reader now requests **at most 64 MiB + 1 byte** and refuses content
larger than the limit, a short read, a size change or trailing data.
The 64 MiB bound is for task metadata, not an application-wide file limit.
It does not prevent every same-size concurrent edit or filesystem race.

Legacy records are now decoded using a small shared
`TaskRestore::decodeLegacyTransactional` helper. It uses the original
`QDataStream::Qt_5_2` compatibility version, stages all records, checks the
stream status and catches decoding errors. Only on complete success are the
existing records replaced. The production restore path uses this helper.
This does **not** change `JobOptions::classVersion` or the `tasks.bin`
format, and it does not turn on Windows DPAPI encryption.

## Added synthetic tests

- Exactly-at-limit file, above-limit file, null output target and dangling symlink.
- Successful two-record restore, truncated second record, decoder exception and
  intentional empty legacy list.
- Earlier tests continue covering absent files, unreadable directories, live
  list rollback and atomic save behavior.

The new `task_restore` QtTest target is registered in CMake.
All tests use synthetic data rather than cloud credentials or private task files.

## Still required before DPAPI integration/release

- Successful Windows and Linux GitHub Actions runs at the exact commit.
- Test actual complete and damaged `JobOptions` files end-to-end, including
  edits, recovery state, and GUI warning behavior.
- Resolve same-size concurrent modification risks, portable mode behavior,
  DPAPI recovery and migration consent/backup policy.
- Resolve the historical Firefox malware classification and unfinished
  third-party rights/licensing review. No public release is authorized.
