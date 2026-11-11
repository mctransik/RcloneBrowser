# Windows proxy credential storage — first implementation (2026-10-14)

**Scope:** non-portable Windows installation; development branch only. **Not a security certification.** Builds and real GUI migration still require validation.

- Proxy URLs with username or password are stored as `CRED_TYPE_GENERIC` entries via Windows `CredWriteW`, with `CRED_PERSIST_LOCAL_MACHINE` (same Windows user on this computer). The Windows registry QSettings entries retain only the URL with userinfo removed, plus a random Credential Manager reference.
- Initial startup attempts to migrate old proxy URLs containing userinfo. The existing value is only replaced after a successful Credential Manager write; errors preserve the old configuration and require user acknowledgement to continue with plaintext legacy settings. An unavailable credential reference causes startup to fail closed when the custom proxy is enabled rather than silently bypass it.
- Editing proxy settings uses the same protected storage. The old Windows credential is deleted only after successfully updating QSettings; the attempted rollback on QSettings error is best-effort (registry writes are not transactional). Failed writes may leave orphaned Credential Manager records; do not claim secure erasure.
- `rclone_updater` reads protected URLs before using them, and blocks checks/updates on a credential read failure. Existing global proxy environment variables and child-process inheritance **remain security work to do**.
- Portable mode retains its historical plaintext format, with a warning requiring explicit confirmation when proxy credentials are embedded. Non-Windows settings are unchanged and **not encrypted** by this change.
- A copied settings file is **not a full backup** of protected credentials. They are user- and machine-scoped; moving machines requires re-entering proxy credentials. Old plaintext may remain in registry backups, snapshots and other copies.
- Tests cover URL sanitization, unchanged legacy loading, and a temporary test credential round-trip on Windows when the CI logon session provides a usable Credential Manager. **A skipped Windows credential test is not a pass of the live storage API**.

**Risks not covered:** malicious same-user processes, raw proxy values in application memory and process environments, saved `tasks.bin` extra arguments, clipboard confirmation once user approves, unredacted transfer logs, Firefox download warning, code signing, and release approvals.

**Acceptance before release:** complete Windows CI including non-skipped Credential Manager round-trip, GUI migration from disposable credentials, Windows portable-mode warning, missing-credential failure and repair, start/restart behavior, proxy authentication connectivity, and no plaintext secrets in QSettings after successful migration.

- Defense against silent credential loss: saving an unchanged, sanitized URL while the referenced credential is unavailable is rejected. The user must re-enter authenticated proxy settings deliberately.
- The updater refuses update checks if a custom proxy is selected but missing or invalid; it must not silently fall back to a direct connection.

## Windows CI diagnostic follow-up (2026-10-14)

Run [#38535052799](https://github.com/mctransik/RcloneBrowser/actions/runs/38535052799) compiled the Windows application and all three test executables but **failed the `proxy_credentials` CTest** (1 of 3 failed). Linux passed all three tests and completed its package successfully. The existing Windows CTest log did not include the failing assertion or Windows error code. The workflow now uses `ctest --verbose --output-on-failure` to show the exact QtTest failure. No credentials from real users are used by the test; the integration path uses disposable dummy data. The actual Windows cause is still **unknown** until the new CI run. Do not waive or reinterpret the failing test as successful.

## Windows CI migration bug diagnosis, 2026-10-14

Runs [#38535052799](https://github.com/mctransik/RcloneBrowser/actions/runs/38535052799) and [#38536628821](https://github.com/mctransik/RcloneBrowser/actions/runs/38536628821) both compiled Windows successfully but failed the `proxy_credentials` runtime test. The latter run enabled CTest verbose output, but QtTest supplied no assertion details to the console. **Code review identified a deterministic migration bug:** `ProxyCredentials::save` returned immediately when the requested URL matched the old value, even when the old value contained credentials and protection was requested. Thus `migrateLegacy` could not migrate unchanged plaintext credentials. The early-return condition now requires an already-protected target, unprotected mode, or an URL with no credentials. The XML test-output file is now printed if the Windows CTest step fails. Until the new CI passes, this is a *reasoned diagnosis*, not yet a verified fix.
