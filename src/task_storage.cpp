#include "task_storage.h"

#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QSaveFile>

namespace TaskStorage {

// A damaged or unexpectedly large saved task file must be kept for recovery,
// not treated as an empty list. Ordinary task files are far smaller than this.
ReadResult readTaskBytes(const QString &path, QByteArray *contents) {
  if (!contents) return ReadResult::Unreadable;
  contents->clear();
  if (path.isEmpty()) return ReadResult::Unreadable;
  const QFileInfo info(path);
  if (!info.exists() && !info.isSymLink())
    return ReadResult::Absent;
  constexpr qint64 maxTaskFileSize = 64LL * 1024 * 1024;
  if (info.isSymLink() || !info.isFile() || info.size() < 0 ||
      info.size() > maxTaskFileSize)
    return ReadResult::Unreadable;
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly))
    return ReadResult::Unreadable;
  QByteArray bytes = file.readAll();
  if (file.error() != QFileDevice::NoError ||
      static_cast<qint64>(bytes.size()) != info.size())
    return ReadResult::Unreadable;
  *contents = std::move(bytes);
  return ReadResult::Loaded;
}

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
