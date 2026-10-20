#include "task_restore.h"

#include <QByteArray>
#include <QString>
#include <QTemporaryDir>
#include <QtTest>
#include <stdexcept>

namespace {
constexpr quint32 kRecordSignature = 0x1452A3B4;
struct SyntheticTask {
  QString name;
};

QByteArray makePayload(const QStringList &names) {
  QByteArray payload;
  QDataStream stream(&payload, QIODevice::WriteOnly);
  stream.setVersion(QDataStream::Qt_5_2);
  for (const QString &name : names)
    stream << name << kRecordSignature;
  return payload;
}

bool decode(const QByteArray &payload, QList<SyntheticTask *> &tasks) {
  return TaskRestore::decodeLegacyTransactional(
      payload, tasks, [](QDataStream &stream, SyntheticTask &task) {
        quint32 signature = 0;
        stream >> task.name >> signature;
        if (stream.status() == QDataStream::Ok &&
            signature != kRecordSignature)
          throw std::runtime_error("invalid synthetic record");
      });
}
} // namespace

class TestTaskRestore : public QObject {
  Q_OBJECT

private slots:
  void validRecordsReplaceDestinationOnlyAfterCompleteRead() {
    QList<SyntheticTask *> live{new SyntheticTask{QStringLiteral("Old")}};
    QVERIFY(decode(makePayload({QStringLiteral("First"), QStringLiteral("Second")}), live));
    QCOMPARE(live.size(), 2);
    QCOMPARE(live.at(0)->name, QStringLiteral("First"));
    QCOMPARE(live.at(1)->name, QStringLiteral("Second"));
    qDeleteAll(live);
  }

  void truncatedSecondRecordPreservesOriginalPointerAndContents() {
    QList<SyntheticTask *> live{new SyntheticTask{QStringLiteral("Original")}};
    SyntheticTask *unchanged = live.first();
    QByteArray payload = makePayload({QStringLiteral("One"), QStringLiteral("Two")});
    payload.chop(3); // First record valid; second one incomplete.
    QVERIFY(!decode(payload, live));
    QCOMPARE(live.size(), 1);
    QVERIFY(live.first() == unchanged);
    QCOMPARE(live.first()->name, QStringLiteral("Original"));
    qDeleteAll(live);
  }

  void decoderExceptionPreservesOriginalList() {
    QList<SyntheticTask *> live{new SyntheticTask{QStringLiteral("Original")}};
    SyntheticTask *unchanged = live.first();
    QByteArray payload = makePayload({QStringLiteral("Corrupt")});
    payload[payload.size() - 1] =
        static_cast<char>(payload.at(payload.size() - 1) ^ 0x01);
    QVERIFY(!decode(payload, live));
    QCOMPARE(live.size(), 1);
    QVERIFY(live.first() == unchanged);
    qDeleteAll(live);
  }

  void emptyLegacyPayloadIsAnIntentionalEmptyList() {
    QList<SyntheticTask *> live{new SyntheticTask{QStringLiteral("Old")}};
    QVERIFY(decode(QByteArray(), live));
    QVERIFY(live.isEmpty());
  }
};

QTEST_GUILESS_MAIN(TestTaskRestore)
#include "test_task_restore.moc"
