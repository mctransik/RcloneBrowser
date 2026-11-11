#include "rclone_args.h"

#include <QProcess>

namespace RcloneArgs {

QStringList splitOptions(const QString &options) {
  // QProcess::splitCommand: whitespace-separated, "..." groups, """ is a
  // literal quote, backslashes are not special.
  QStringList out;
  for (const QString &arg : QProcess::splitCommand(options)) {
    if (!arg.isEmpty()) {
      out << arg;
    }
  }
  return out;
}

} // namespace RcloneArgs
