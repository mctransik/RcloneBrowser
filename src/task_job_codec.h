#pragma once

#include "job_options.h"
#include <QDataStream>

// The serialized legacy JobOptions record format, shared by production
// tasks.bin and regression tests. Keep Qt_5_2 and classVersion unchanged.
namespace TaskJobCodec {
QDataStream &write(QDataStream &stream, const JobOptions &record);
QDataStream &read(QDataStream &stream, JobOptions &record);
} // namespace TaskJobCodec
