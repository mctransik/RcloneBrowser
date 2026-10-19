#include "task_storage.h"

#include <QFileDevice>
#include <QSaveFile>

namespace TaskStorage {

bool saveAtomically(
    const QString &path,
    const std::function<bool(QDataStream &)> &serialize) {
  if (path.isEmpty() || !serialize)
    return false;

  QSaveFile file(path);
  // Never fall back to an in-place truncate/write if atomic replace cannot
  // be guaranteed (e.g. unusual removable or network filesystems).
  file.setDirectWriteFallback(false);
  if (!file.open(QIODevice::WriteOnly))
    return false;

#ifndef Q_OS_WIN
  // The task stream can contain credential-bearing custom rclone flags.
  // QSaveFile may inherit permissions from an existing, formerly public file;
  // restrict the *temporary* file before any serialized data is written.
  if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                           QFileDevice::ReadUser | QFileDevice::WriteUser)) {
    file.cancelWriting();
    return false;
  }
#endif

  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_5_2); // compatibility with existing tasks.bin
  if (!serialize(stream) || stream.status() != QDataStream::Ok) {
    file.cancelWriting();
    return false;
  }
  return file.commit();
}

} // namespace TaskStorage
