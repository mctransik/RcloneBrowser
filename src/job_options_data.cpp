#include "job_options.h"

JobOptions::JobOptions(bool isDownload) : JobOptions() {
  setJobType(isDownload);
  uniqueId = QUuid::createUuid();
}

JobOptions::JobOptions()
    : jobType(UnknownJobType), operation(UnknownOp), dryRun(false), sync(false),
      syncTiming(UnknownTiming), skipNewer(false), skipExisting(false),
      compare(false), compareOption(), verbose(false), sameFilesystem(false),
      dontUpdateModified(false), maxDepth(0), deleteExcluded(false),
      isFolder(false) {}

const qint32 JobOptions::classVersion = 3;

JobOptions::~JobOptions() {}

SerializationException::SerializationException(QString msg)
    : QException(), Message(msg) {}
