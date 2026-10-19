#pragma once

#include <QByteArray>
#include <QDataStream>
#include <QString>
#include <functional>

namespace TaskStorage {

// Separate an intentionally absent task file from an existing file that
// cannot be read safely. Never treat a load error as an empty task list.
enum class ReadResult { Absent, Loaded, Unreadable };
ReadResult readTaskBytes(const QString &path, QByteArray *contents);

// Persist existing QDataStream format without truncating the old tasks.bin.
// The callback must write only to the supplied QDataStream, never to the path.
// No encryption is claimed: portable task metadata can still contain secrets.
bool saveAtomically(
    const QString &path,
    const std::function<bool(QDataStream &)> &serialize);

} // namespace TaskStorage
