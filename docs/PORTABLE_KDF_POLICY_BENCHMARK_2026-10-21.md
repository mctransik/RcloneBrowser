# Argon2id KDF policy and bounded benchmark — security review (2026-10-21)

**Experimental research and CI regression tests only. No approved production
password profile; no application integration or `tasks.bin` migration.**

## Why this gate exists

The `RBTSENC2` draft stores Argon2id parameters inside the binary header.
Before authentication they are **attacker-controlled bytes**. Blindly passing
them to `crypto_pwhash()` could exhaust CPU/RAM or accept a weak work-factor
downgrade. The initial research-format **revision 1** therefore allows only
its one *test-fixture* profile (`ops=2`, `mem=65536 KiB`, Argon2id13).
These settings are **not an authorization to ship that profile**.

A new test case, `hostileHeadersNeverInvokeArgon2id`, records the number of
calls to the research KDF wrapper. It modifies each version/algorithm/KDF/
length field, injects plausible-but-disallowed values, all-ones cost values,
truncation, trailing bytes and oversized buffers, and asserts that the KDF
call counter never advances. `authorizedHeaderDerivesOnlyAfterValidation`
verifies that one valid read *does* invoke the derivation exactly once.
The existing AEAD tamper/wrong-password tests remain part of this suite.

CI compiles and executes the envelope fixture on **both Windows and Linux**.
The format constants also carry a compile-time assertion: if a libsodium
preset changes, the already versioned research encoding cannot silently
change and invalidate the independently pinned cross-platform byte fixture.

## Upstream work-factor profiles

| Profile | Argon2id operations | RAM requested | Status |
| --- | ---: | ---: | --- |
| libsodium INTERACTIVE | 2 | 64 MiB | **Development fixture only** |
| libsodium MODERATE | 3 | 256 MiB | **Benchmark candidate; not chosen yet** |

The libsodium documentation currently lists these interactive/moderate
profiles. A future production profile may vary after real-hardware
measurement and independent review. It **must not** silently adjust itself
downwards when it encounters low RAM, an older OS, or an intentionally
edited encrypted file. Readers should validate the recorded work factors
against an explicitly approved, versioned policy **before** calling Argon2id.

## Compile-only CI / opt-in, user-data-free benchmarking

File: `tests/benchmark_portable_kdf.cpp`. It is built **only** when
`RB_BUILD_PORTABLE_CRYPTO_PROBE=ON`, next to the isolated crypto tests.
It is **not** registered as CTest and therefore is **never benchmarked by CI**.
No target installs this tool into a user-facing application or release ZIP.
No system credential, Windows profile, real password or task file is read.

A developer can run this synthetic benchmark explicitly:

- Windows, from a prepared local developer build:
  `build/tests/portable_kdf_benchmark.exe --run-synthetic`
- Linux, from a prepared local developer build:
  `build/tests/portable_kdf_benchmark --run-synthetic`

The tool uses Argon2id13 via upstream libsodium with synthetic, public
password bytes and a fresh random salt. It measures three iterations of
each of INTERACTIVE and MODERATE, sequentially; prints the min/median/max
elapsed time and exact work-factor values, but **no salts, keys, passwords,
or task contents**. It refuses preset changes, greater than 256 MiB of
requested KDF RAM, more than three Argon2id operations, or allocation
failures. Never auto-retry under a weaker profile. It requires an explicit
`--run-synthetic` argument.

### Separate Windows research artifact (manual workflow only)

The stacked research branch
`security/portable-kdf-local-benchmark-2026-10-23` adds a deliberately
**separate** Windows-only developer artifact for voluntary local measurement.
It is not embedded into RcloneBrowser's application/release ZIP.

The diagnostic is packaged only when the `Update` workflow is **manually
dispatched on this exact branch**, after the executable's `--help` startup
check. It includes the test-only `libsodium.dll`, its upstream ISC license,
the source commit, the user instructions, and a SHA-256 inventory. The
existing Windows-package ClamAV job additionally checks the research
inventory, scans this independent payload, and **only then** uploads
`RESEARCH-ONLY-portable-kdf-windows`.

Neither hosted CI nor any app startup runs `--run-synthetic`. It is strictly
a manual choice of the person running the benchmark on their own PC. Hashes
from inside the same downloaded artifact detect accidental changes but do
**not** independently establish provenance; the uploaded executable has
no Authenticode signature. Do not disable Windows security protections to
run it. Consult:
`docs/PORTABLE_KDF_LOCAL_MEASUREMENT_2026-10-23.md`.

The resulting measurements must not be taken as approval of the wire
format or a particular KDF. The research tool may require a supported
Microsoft Visual C++ runtime on Windows; missing DLL errors are reasons
to stop and review dependencies rather than download random DLLs.

Measurements should be repeated on representative supported devices,
including a lower-end Windows PC and Linux systems. Review memory pressure,
process concurrency, GUI responsiveness, cold/warm variance and acceptable
password-unlock latency. Three samples are exploratory, **not a reliable
benchmark for production sizing**. Avoid high work-factor stress on resource
constrained machines.

## Open policy decisions

1. Select the minimum acceptable Argon2id cost after real-device profiling;
   explicitly specify a bounded set of supported (version, algorithm,
   operations, memory) tuples. Stronger unreviewed values must also be
   refused to prevent untrusted DoS.
2. Decide a separate revision or upgrade/migration method for updating
   the work factors while preserving an existing portable backup. The
   existing immutable deterministic fixture must remain readable.
3. Specify password input bytes (Unicode normalization, UTF-8, accepted
   length and password manager paste behavior). Do not assume that
   `QString` and raw `QByteArray` represent the same password automatically
   across all operating systems.
4. Keep keys out of logs and minimize lifetime in memory; investigate
   secure memory handling and Qt implicit-sharing limitations.
5. Define task-file size policy and total process resource budgets,
   including multiple concurrent unlock attempts. Reject malformed
   headers before *any* KDF work, allocation proportional to a claimed
   length, or external side effects.
6. Independently review the envelope and adversarial parser, test rollback,
   wrong-password, corruption, legacy-downgrade refusal, interrupted writes
   and synthetic cross-OS backup recovery before any real-file migration.

Sources:
- https://doc.libsodium.org/password_hashing/default_phf
- https://doc.libsodium.org/secret-key_cryptography/aead/chacha20-poly1305/xchacha20-poly1305_construction
- `docs/PORTABLE_V2_ENVELOPE_WIRE_CANDIDATE_2026-10-21.md`

**No production encryption, no public release and no automatic migration
should be enabled by this research PR.**
