#pragma once

// Pure decision logic used by RcloneUpdater: version parsing, the
// "rclone selfupdate" command line, proxy validation for the updater process
// and interpretation of selfupdate's output. No network, GUI, settings or
// process access, so every rule is covered by unit tests
// (tests/test_update_check.cpp).

#include <QString>
#include <QStringList>
#include <QUrl>

namespace UpdateCheck {

// -1 if a < b, 0 if equal, 1 if a > b. Accepts "v1.2.3", "1.2.3-beta...",
// "rclone v1.2.3"; missing minor/patch parts count as 0.
int compareVersions(const QString &a, const QString &b);

// First "vX.Y.Z" in text (version.txt content or a GitHub release URL), or
// an empty string.
QString findVersionTag(const QString &text);

// True only for an exact stable tag such as "v1.75.1" (no prefix, suffix,
// beta marker or whitespace). install() refuses anything else.
bool isStableVersionTag(const QString &version);

// Arguments for "rclone selfupdate": a stable release, verified by rclone
// (SHA256SUMS + PGP signature), written to outputPath instead of replacing
// the running binary.
QStringList selfupdateArguments(const QString &version,
                                const QString &outputPath);

// Proxy for the updater process from the configured https/http proxy
// (https preferred). Returns an invalid QUrl if useProxy is false. Sets
// *invalid when a proxy is configured but unusable (not http/https, no host);
// the update must then not be attempted.
QUrl updaterProxy(bool useProxy, const QString &httpsProxy,
                  const QString &httpProxy, bool *invalid);

// rclone selfupdate exits successfully without writing --output when the
// requested version is already running; it then prints "rclone is up to date".
bool reportsUpToDate(const QString &output);

} // namespace UpdateCheck
