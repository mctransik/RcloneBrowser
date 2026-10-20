#pragma once

#include <QBuffer>
#include <QDataStream>
#include <QList>
#include <QtAlgorithms>

#include <memory>
#include <utility>

namespace TaskRestore {

// Decode all legacy Qt_5_2 records before changing the live list.
// ReadOne must consume exactly one record and may throw on invalid data.
// Failed reads leave destination and its existing record pointers unchanged.
// Format identification and file-access failures are checked by the caller.
template <typename Record, typename ReadOne>
bool decodeLegacyTransactional(const QByteArray &payload,
                               QList<Record *> &destination,
                               ReadOne readOne) {
  QBuffer input;
  input.setData(payload);
  if (!input.open(QIODevice::ReadOnly))
    return false;

  QDataStream stream(&input);
  stream.setVersion(QDataStream::Qt_5_2);
  QList<Record *> staged;

  while (!stream.atEnd()) {
    std::unique_ptr<Record> record = std::make_unique<Record>();
    try {
      readOne(stream, *record);
    } catch (...) {
      qDeleteAll(staged);
      return false;
    }
    if (stream.status() != QDataStream::Ok) {
      qDeleteAll(staged);
      return false;
    }
    staged.append(record.release());
  }

  if (stream.status() != QDataStream::Ok) {
    qDeleteAll(staged);
    return false;
  }

  qDeleteAll(destination);
  destination = std::move(staged);
  return true;
}

} // namespace TaskRestore
