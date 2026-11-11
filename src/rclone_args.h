#pragma once

// Turning user-entered option strings ("--fast-list --exclude "My Files/**"")
// into rclone arguments. Covered by tests/test_rclone_args.cpp.

#include <QString>
#include <QStringList>

namespace RcloneArgs {

// Splits a line of extra rclone options into separate arguments.
// Whitespace separates arguments; repeated spaces never produce empty
// arguments (which rclone would treat as an extra source/destination).
// Double quotes group text containing spaces, e.g.
//   --exclude "My Files/**"   ->   "--exclude", "My Files/**"
// Backslashes are kept literally, so Windows paths work unchanged.
// Empty arguments (e.g. from "") are dropped.
QStringList splitOptions(const QString &options);

} // namespace RcloneArgs
