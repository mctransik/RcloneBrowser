#pragma once

#include <QDataStream>
#include <QString>
#include <functional>

namespace TaskStorage {

// Persist existing QDataStream format without truncating the old tasks.bin.
// The callback must write only to the supplied QDataStream, never to the path.
// No encryption is claimed: portable task metadata can still contain secrets.
bool saveAtomically(
    const QString &path,
    const std::function<bool(QDataStream &)> &serialize);

} // namespace TaskStorage
