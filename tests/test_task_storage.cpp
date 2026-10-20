#include "task_storage.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestTaskStorage : public QObject {
  Q_OBJECT

private slots:
  void missingTaskFileIsNotAReadFailure() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QByteArray content("stale");
    QCOMPARE(TaskStorage::readTaskBytes(temp.filePath("missing-tasks.bin"),
                                        &content),
             TaskStorage::ReadResult::Absent);
    QVERIFY(content.isEmpty());
  }

  void existingTaskFileIsReadWithoutModification() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath("tasks.bin");
    const QByteArray original("legacy\0task", 11);
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write(original), static_cast<qint64>(original.size()));
    file.close();
    QByteArray output;
    QCOMPARE(TaskStorage::readTaskBytes(path, &output),
             TaskStorage::ReadResult::Loaded);
    QCOMPARE(output, original);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), original);
  }

  void emptyTaskFileIsValidNotMissing() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath("tasks.bin");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    QByteArray bytes("stale");
    QCOMPARE(TaskStorage::readTaskBytes(path, &bytes),
             TaskStorage::ReadResult::Loaded);
    QVERIFY(bytes.isEmpty());
  }

  void rejectsDirectoryInsteadOfTreatingItAsMissing() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QByteArray bytes("old");
    QCOMPARE(TaskStorage::readTaskBytes(temp.path(), &bytes),
             TaskStorage::ReadResult::Unreadable);
    QVERIFY(bytes.isEmpty());
  }

  void acceptsFileAtExactSafetyBoundary() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath("tasks.bin");
    constexpr qint64 limit = 64LL * 1024 * 1024;
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.resize(limit));
    file.close();

    QByteArray bytes;
    QCOMPARE(TaskStorage::readTaskBytes(path, &bytes),
             TaskStorage::ReadResult::Loaded);
    QCOMPARE(static_cast<qint64>(bytes.size()), limit);
    QCOMPARE(bytes.at(0), char(0));
    QCOMPARE(bytes.at(bytes.size() - 1), char(0));
  }

  void nullDestinationIsRejected() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    QCOMPARE(TaskStorage::readTaskBytes(temp.filePath("tasks.bin"), nullptr),
             TaskStorage::ReadResult::Unreadable);
  }

  void rejectsOversizedFileForRecovery() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString path = temp.filePath("tasks.bin");
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.resize(64LL * 1024 * 1024 + 1));
    file.close();
    QByteArray bytes;
    QCOMPARE(TaskStorage::readTaskBytes(path, &bytes),
             TaskStorage::ReadResult::Unreadable);
    QVERIFY(bytes.isEmpty());
  }

#ifndef Q_OS_WIN
  void rejectsBrokenSymlinkInsteadOfTreatingItAsMissing() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString target = temp.filePath("target.bin");
    const QString link = temp.filePath("tasks.bin");
    QFile file(target);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("task", 4), qint64(4));
    file.close();
    if (!QFile::link(target, link))
      QSKIP("Filesystem does not permit symlinks");
    QVERIFY(QFile::remove(target));
    QByteArray bytes;
    QCOMPARE(TaskStorage::readTaskBytes(link, &bytes),
             TaskStorage::ReadResult::Unreadable);
    QVERIFY(bytes.isEmpty());
  }

  void rejectsSymlinkedTaskFile() {
    QTemporaryDir temp;
    QVERIFY(temp.isValid());
    const QString original = temp.filePath("target.bin");
    const QString symlink = temp.filePath("tasks.bin");
    QFile file(original);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QCOMPARE(file.write("legacy", 6), qint64(6));
    file.close();
    if (!QFile::link(original, symlink))
      QSKIP("Filesystem does not permit symlinks");
    QByteArray bytes;
    QCOMPARE(TaskStorage::readTaskBytes(symlink, &bytes),
             TaskStorage::ReadResult::Unreadable);
    QVERIFY(bytes.isEmpty());
  }
#endif

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
