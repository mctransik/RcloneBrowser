# Real JobOptions codec and corruption tests (2026-10-20)

**Development-only draft. Windows/Linux CI and interactive GUI checks pending.**
Builds on the bounded-read changes in PR #8, the read-only recovery design
in PR #7 and the protected-envelope detection in PR #6.

## Scope and compatibility

Previously the production task record serializer and decoder were static
functions in `list_of_job_options.cpp`, making tests of real saved records
difficult. This change relocates the **same legacy Qt_5_2 binary field order**
into `TaskJobCodec::write/read` and routes production persistence and
transactional restore through those functions.

The small `JobOptions` data constructor/destructor and
`SerializationException` definitions are separated from rclone argument
generation so the regression target can instantiate genuine production
`JobOptions` records without launching a GUI or cloud process.

- The on-disk record version stays **3**, and existing Qt_5_2 field ordering
  is preserved. Version 1 and version 2 records are still accepted.
- Reader explicitly refuses record versions below 1 or above 3.
- Four legacy 32-bit task enums are now decoded as separate integers and
  checked against their declared enum ranges before being assigned. This
  replaces old C++ enum-reference aliasing.
- The task data constructor initializes `DriveSharedWithMe` to false
  rather than leaving that persisted field uninitialized.
- The read-only restoration gate and transactional all-or-nothing behavior
  from earlier draft PRs remain in place. Protected envelope files are
  still **not** decrypted or migrated.

## New real-record regression target

`test_task_job_codec` (QtTest) uses only invented paths and flags:
- Complete version-3 records roundtrip byte-for-byte through the production
  `TaskJobCodec` including source, destination, extra options and UUID.
- Independently built version-1 and version-2 legacy fixtures decode.
- A truncated second **real** record, invalid class, unsupported versions,
  invalid job-type enum and trailing incomplete data must be rejected.
- On failure, existing live pointers and task names are preserved.

It does **not** use existing users' files, credentials or cloud services.

## Outstanding gates

- Windows/Linux CI on the exact new commit, review for file format fidelity
  and a manual GUI test using only synthetic files.
- More exhaustive malformed field/fuzz tests, including oversized strings,
  corrupted values, old records, and read-only recovery interactions.
- Explicit user-approved, backup-safe DPAPI migration; no automatic
  conversion, decryption fallback or portable-mode DPAPI.
- Firefox classification issue and complete third-party license/signing
  review before any release. Automated release remains disabled.
