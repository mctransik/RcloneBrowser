#include "task_job_codec.h"
#include "task_restore.h"

#include <QBuffer>
#include <QByteArray>
#include <QList>
#include <QString>
#include <QtTest>

namespace {

JobOptions makeTask(const QString &name) {
  JobOptions task(true);
  task.description = name;
  task.jobType = JobOptions::Download;
  task.operation = JobOptions::Copy;
  task.sync = true;
  task.syncTiming = JobOptions::After;
  task.compare = true;
  task.compareOption = JobOptions::Checksum;
  task.transfers = QStringLiteral("4");
  task.checkers = QStringLiteral("8");
  task.excluded = QStringLiteral("*.tmp\n*.bak");
  task.extra = QStringLiteral("--test-only-option");
  task.source = QStringLiteral("synthetic-cloud:folder");
  task.dest = QStringLiteral("/synthetic/target");
  task.isFolder = true;
  task.DriveSharedWithMe = true;
  return task;
}

QByteArray encode(const QList<JobOptions *> &tasks) {
  QByteArray bytes;
  QDataStream stream(&bytes, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_5_2);
  for (const JobOptions *item : tasks)
    TaskJobCodec::write(stream, *item);
  return stream.status() == QDataStream::Ok ? bytes : QByteArray();
}

bool decode(const QByteArray &bytes, QList<JobOptions *> &destination) {
  return TaskRestore::decodeLegacyTransactional(
      bytes, destination, [](QDataStream &stream, JobOptions &task) {
        TaskJobCodec::read(stream, task);
      });
}

// Build independent fixtures of the historical Qt_5_2 layout (v1 and v2).
// Fields predating v3 must remain readable without a conversion step.
QByteArray olderVersionFixture(qint32 version) {
  JobOptions jo = makeTask(QStringLiteral("Historical fixture"));
  QByteArray bytes;
  QDataStream s(&bytes, QIODevice::WriteOnly);
  s.setVersion(QDataStream::Qt_5_2);
  s << jo.myName() << version << jo.description
    << jo.jobType << jo.operation << jo.sync
    << jo.syncTiming << jo.skipNewer << jo.skipExisting << jo.compare
    << jo.compareOption << jo.verbose << jo.sameFilesystem
    << jo.dontUpdateModified << jo.transfers << jo.checkers << jo.bandwidth
    << jo.minSize << jo.minAge << jo.maxAge << jo.maxDepth
    << jo.connectTimeout << jo.idleTimeout << jo.retries
    << jo.lowLevelRetries << jo.deleteExcluded << jo.excluded << jo.extra
    << jo.DriveSharedWithMe << jo.source << jo.dest;
  if (version >= 2)
    s << jo.isFolder;
  return bytes;
}

} // namespace

class TestTaskJobCodec : public QObject {
  Q_OBJECT

private slots:
  void fullVersion3RecordsRoundTripByteForByte() {
    JobOptions a = makeTask(QStringLiteral("First"));
    JobOptions b = makeTask(QStringLiteral("Second"));
    b.jobType = JobOptions::Upload;
    b.operation = JobOptions::Sync;
    b.compareOption = JobOptions::SizeOnly;
    b.extra = QStringLiteral("--synthetic-only");
    b.DriveSharedWithMe = false;
    QList<JobOptions *> source{&a, &b};
    const QByteArray original = encode(source);
    QVERIFY(!original.isEmpty());

    QList<JobOptions *> restored;
    QVERIFY(decode(original, restored));
    QCOMPARE(restored.size(), 2);
    QCOMPARE(restored.at(0)->description, a.description);
    QCOMPARE(restored.at(0)->source, a.source);
    QCOMPARE(restored.at(0)->dest, a.dest);
    QCOMPARE(restored.at(0)->extra, a.extra);
    QCOMPARE(restored.at(0)->uniqueId, a.uniqueId);
    QCOMPARE(restored.at(1)->operation, JobOptions::Sync);
    QCOMPARE(restored.at(1)->jobType, JobOptions::Upload);
    QCOMPARE(restored.at(1)->DriveSharedWithMe, false);
    QCOMPARE(encode(restored), original);
    qDeleteAll(restored);
  }

  void acceptsHistoricalVersionOneAndTwo() {
    for (qint32 version : {qint32(1), qint32(2)}) {
      QList<JobOptions *> restored;
      QVERIFY(decode(olderVersionFixture(version), restored));
      QCOMPARE(restored.size(), 1);
      QCOMPARE(restored.first()->description,
               QStringLiteral("Historical fixture"));
      QCOMPARE(restored.first()->source,
               QStringLiteral("synthetic-cloud:folder"));
      QCOMPARE(restored.first()->extra, QStringLiteral("--test-only-option"));
      QCOMPARE(restored.first()->isFolder, version == 2);
      QVERIFY(restored.first()->uniqueId.isNull());
      qDeleteAll(restored);
    }
  }

  void truncatedSecondRealRecordDoesNotReplaceExistingList() {
    JobOptions first = makeTask(QStringLiteral("Valid record"));
    JobOptions second = makeTask(QStringLiteral("Truncated record"));
    QByteArray truncated = encode(QList<JobOptions *>{&first, &second});
    truncated.chop(5);

    auto *existing = new JobOptions(makeTask(QStringLiteral("Original")));
    QList<JobOptions *> live{existing};
    QVERIFY(!decode(truncated, live));
    QCOMPARE(live.size(), 1);
    QVERIFY(live.first() == existing);
    QCOMPARE(existing->description, QStringLiteral("Original"));
    qDeleteAll(live);
  }

  void rejectsWrongClassAndUnsupportedVersionsWithoutLosingExistingTask() {
    JobOptions valid = makeTask(QStringLiteral("Valid"));
    const QByteArray validRecord = encode(QList<JobOptions *>{&valid});
    for (const QByteArray &bad : {
             QByteArray(olderVersionFixture(0)),
             QByteArray(olderVersionFixture(4)),
             QByteArray()}) {
      QByteArray payload = validRecord + bad;
      if (bad.isEmpty()) {
        QDataStream writer(&payload, QIODevice::Append);
        writer.setVersion(QDataStream::Qt_5_2);
        writer << QStringLiteral("WrongClass") << qint32(3);
      }
      auto *existing = new JobOptions(makeTask(QStringLiteral("Original")));
      QList<JobOptions *> live{existing};
      QVERIFY(!decode(payload, live));
      QCOMPARE(live.size(), 1);
      QVERIFY(live.first() == existing);
      qDeleteAll(live);
    }
  }

  void corruptOperationOrJobTypeIsRejected() {
    JobOptions job = makeTask(QStringLiteral("Enum validation"));
    QByteArray payload = encode(QList<JobOptions *>{&job});
    QBuffer buffer(&payload);
    QVERIFY(buffer.open(QIODevice::ReadWrite));
    QDataStream reader(&buffer);
    reader.setVersion(QDataStream::Qt_5_2);
    QString className, description;
    qint32 version = 0;
    reader >> className >> version >> description;
    QCOMPARE(reader.status(), QDataStream::Ok);
    QCOMPARE(className, QStringLiteral("JobOptions"));
    const qint64 enumOffset = buffer.pos();
    QVERIFY(buffer.seek(enumOffset));
    QDataStream writer(&buffer);
    writer.setVersion(QDataStream::Qt_5_2);
    writer << quint32(0xffffffff);

    QList<JobOptions *> destination;
    QVERIFY(!decode(payload, destination));
    QVERIFY(destination.isEmpty());
  }

  void trailingBrokenRecordDoesNotPartiallyRestore() {
    JobOptions valid = makeTask(QStringLiteral("Good"));
    QByteArray payload = encode(QList<JobOptions *>{&valid});
    payload.append(char(0x01));
    QList<JobOptions *> restored;
    QVERIFY(!decode(payload, restored));
    QVERIFY(restored.isEmpty());
  }

  void emptyLegacyStreamIsAnEmptyList() {
    QList<JobOptions *> destination;
    QVERIFY(decode(QByteArray(), destination));
    QVERIFY(destination.isEmpty());
  }
};

QTEST_GUILESS_MAIN(TestTaskJobCodec)
#include "test_task_job_codec.moc"
