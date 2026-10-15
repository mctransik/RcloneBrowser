#include "task_storage.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestTaskStorage : public QObject {
  Q_OBJECT

private slots:
  void savesExistingDataStreamFormat() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath("tasks.bin");
    QVERIFY(TaskStorage::saveAtomically(
        path, [](QDataStream &stream) {
          stream << QStringLiteral("task-name") << qint32(3);
          return true;
        }));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_5_2);
    QString name;
    qint32 version = 0;
    stream >> name >> version;
    QCOMPARE(stream.status(), QDataStream::Ok);
    QCOMPARE(name, QStringLiteral("task-name"));
    QCOMPARE(version, qint32(3));
  }

  void failedSerializationPreservesPreviousTaskFile() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath("tasks.bin");
    QVERIFY(TaskStorage::saveAtomically(
        path, [](QDataStream &stream) {
          stream << QStringLiteral("old task");
          return true;
        }));
    QFile original(path);
    QVERIFY(original.open(QIODevice::ReadOnly));
    const QByteArray before = original.readAll();
    original.close();

    QVERIFY(!TaskStorage::saveAtomically(
        path, [](QDataStream &stream) {
          stream << QStringLiteral("interrupted write");
          return false; // simulated serializer failure
        }));
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), before);
    original.close();
  }

  void invalidPathDoesNotCreateTaskFile() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath("missing/tasks.bin");
    QVERIFY(!TaskStorage::saveAtomically(
        path, [](QDataStream &stream) {
          stream << QStringLiteral("never written");
          return true;
        }));
    QVERIFY(!QFile::exists(path));
  }

#ifndef Q_OS_WIN
  void fixesInsecureExistingFilePermissions() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath("tasks.bin");
    QFile legacy(path);
    QVERIFY(legacy.open(QIODevice::WriteOnly));
    QCOMPARE(legacy.write("old", 3), qint64(3));
    legacy.close();
    QVERIFY(legacy.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner |
                                  QFileDevice::ReadUser | QFileDevice::WriteUser |
                                  QFileDevice::ReadGroup | QFileDevice::ReadOther));
    QVERIFY(TaskStorage::saveAtomically(
        path, [](QDataStream &stream) {
          stream << QStringLiteral("protected task");
          return true;
        }));
    const QFileDevice::Permissions permissions = QFile::permissions(path);
    QVERIFY(!(permissions & (QFileDevice::ReadGroup | QFileDevice::WriteGroup |
                             QFileDevice::ExeGroup | QFileDevice::ReadOther |
                             QFileDevice::WriteOther | QFileDevice::ExeOther)));
  }
#endif
};

QTEST_GUILESS_MAIN(TestTaskStorage)
#include "test_task_storage.moc"
