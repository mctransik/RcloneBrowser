#include "task_job_codec.h"

namespace {

// The original stream stores these enums as 32-bit values. Decode through a
// separate integer rather than casting an enum reference to quint32&, which
// violates C++ strict aliasing. Refuse invalid values before they reach tasks.
template <typename Enum>
QDataStream &readEnum(QDataStream &stream, Enum &value, quint32 maximum) {
  static_assert(sizeof(Enum) == sizeof(quint32),
                "Legacy task enums must stay 32 bits");
  quint32 raw = 0;
  stream >> raw;
  if (stream.status() == QDataStream::Ok) {
    if (raw > maximum)
      throw SerializationException(QStringLiteral("invalid task enum"));
    value = static_cast<Enum>(raw);
  }
  return stream;
}

QDataStream &operator>>(QDataStream &s, JobOptions::Operation &e) {
  return readEnum(s, e, JobOptions::Sync);
}
QDataStream &operator>>(QDataStream &s, JobOptions::SyncTiming &e) {
  return readEnum(s, e, JobOptions::UnknownTiming);
}
QDataStream &operator>>(QDataStream &s, JobOptions::CompareOption &e) {
  return readEnum(s, e, JobOptions::ChecksumIgnoreSize);
}
QDataStream &operator>>(QDataStream &s, JobOptions::JobType &e) {
  return readEnum(s, e, JobOptions::Download);
}
} // namespace

namespace TaskJobCodec {

QDataStream &write(QDataStream &stream, const JobOptions &jo) {
  stream << jo.myName() << JobOptions::classVersion << jo.description
         << jo.jobType << jo.operation << /* jo.dryRun <<*/ jo.sync
         << jo.syncTiming << jo.skipNewer << jo.skipExisting << jo.compare
         << jo.compareOption << jo.verbose << jo.sameFilesystem
         << jo.dontUpdateModified << jo.transfers << jo.checkers << jo.bandwidth
         << jo.minSize << jo.minAge << jo.maxAge << jo.maxDepth
         << jo.connectTimeout << jo.idleTimeout << jo.retries
         << jo.lowLevelRetries << jo.deleteExcluded << jo.excluded << jo.extra
         << jo.DriveSharedWithMe << jo.source << jo.dest << jo.isFolder
         << jo.uniqueId;

  return stream;
}

QDataStream &read(QDataStream &stream, JobOptions &jo) {
  QString actualName;
  qint32 actualVersion = 0;
  stream >> actualName >> actualVersion;
  if (stream.status() != QDataStream::Ok)
    return stream;

  if (actualName != jo.myName())
    throw SerializationException(QStringLiteral("incorrect task class"));
  if (actualVersion < 1 || actualVersion > JobOptions::classVersion)
    throw SerializationException(QStringLiteral("unsupported task version"));

  stream >> jo.description >> jo.jobType >> jo.operation >>
      /* jo.dryRun >> */ jo.sync >> jo.syncTiming >> jo.skipNewer >>
      jo.skipExisting >> jo.compare >> jo.compareOption >> jo.verbose >>
      jo.sameFilesystem >> jo.dontUpdateModified >> jo.transfers >>
      jo.checkers >> jo.bandwidth >> jo.minSize >> jo.minAge >> jo.maxAge >>
      jo.maxDepth >> jo.connectTimeout >> jo.idleTimeout >> jo.retries >>
      jo.lowLevelRetries >> jo.deleteExcluded >> jo.excluded >> jo.extra >>
      jo.DriveSharedWithMe >> jo.source >> jo.dest;

  // Keep compatibility with older saved tasks.
  if (actualVersion >= 2) {
    stream >> jo.isFolder;
    if (actualVersion >= 3)
      stream >> jo.uniqueId;
  }
  return stream;
}
} // namespace TaskJobCodec
