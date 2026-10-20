#include "list_of_job_options.h"
#include "task_storage.h"
#include "task_restore.h"
#include "task_job_codec.h"
#include "task_protection.h"
#include "task_list_transaction.h"
#include <QDataStream>
#include <QMessageBox>
#include <QTimer>
#include <qdir.h>
#include <qlogging.h>
#include <qstandardpaths.h>
#include <utils.h>

ListOfJobOptions *ListOfJobOptions::SavedJobOptions = nullptr;
const QString ListOfJobOptions::persistenceFileName = "tasks.bin";

ListOfJobOptions::ListOfJobOptions() {}

ListOfJobOptions *ListOfJobOptions::getInstance() {
  if (SavedJobOptions == nullptr) {
    SavedJobOptions = new ListOfJobOptions();
    if (!RestoreFromUserData(*SavedJobOptions)) {
      // Defer the warning until the event loop can show a dialog. Keep task
      // saving disabled: an unreadable file must not become an empty tasks.bin.
      QTimer::singleShot(0, SavedJobOptions, [] {
        QMessageBox::warning(
            nullptr, QObject::tr("Saved tasks locked"),
            QObject::tr("Saved tasks could not be safely loaded. Existing task "
                        "data has not been changed. Saving and deleting saved "
                        "tasks are disabled until the file is recovered. "
                        "Check the original file or restore a trusted backup."));
      });
    }
  }
  return SavedJobOptions;
}

bool ListOfJobOptions::Persist(JobOptions *jo) {
  if (recoveryReadOnly) return false;
  // Success means the new/edited task was written to disk, NOT "is new".
  return TaskListTransaction::persist(tasks, jo, [this]() {
    return PersistToUserData();
  });
}

bool ListOfJobOptions::Forget(JobOptions *jo) {
  if (recoveryReadOnly) return false;
  // Restore the item and its original position if the disk save fails.
  return TaskListTransaction::forget(tasks, jo, [this]() {
    return PersistToUserData();
  });
}

QString ListOfJobOptions::GetPersistenceFilePath() {

  QDir outputDir;

  if (IsPortableMode()) {
    // in portable mode tasks' file will be saved in the same folder as
    // excecutable
#ifdef Q_OS_MACOS
    // on macOS excecutable file is located in
    // ./rclone-browser.app/Contents/MasOS/
    // to get actual bundle folder we have
    // to traverse three levels up
    outputDir = QDir(qApp->applicationDirPath() + "/../../..");
#else
#ifdef Q_OS_WIN
    // not macOS
    outputDir = QDir(qApp->applicationDirPath());
#else
    QString xdg_config_home = qgetenv("XDG_CONFIG_HOME");
    outputDir = QDir(xdg_config_home + "/rclone-browser");
#endif
#endif

  } else {

    // get data location folder from Qt  - OS dependend
    outputDir =
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
  }

  if (!outputDir.exists() && !outputDir.mkpath("."))
    return QString();
  return outputDir.absoluteFilePath(persistenceFileName);
}

QFile *ListOfJobOptions::GetPersistenceFile(QIODevice::OpenModeFlag mode) {
  const QString filePath = GetPersistenceFilePath();
  if (filePath.isEmpty())
    return nullptr;
  QFile *file = new QFile(filePath);

  if (!file->open(mode)) {
    //    qDebug() << QString("Could not open ") << file->fileName();
    delete file;
    file = nullptr;
  }
  return file;
}

bool ListOfJobOptions::RestoreFromUserData(ListOfJobOptions &dataIn) {
  const QString path = GetPersistenceFilePath();
  QByteArray stored;
  const TaskStorage::ReadResult read = TaskStorage::readTaskBytes(path, &stored);
  if (read == TaskStorage::ReadResult::Absent)
    return true; // A genuinely missing file means a new empty task list.
  if (read != TaskStorage::ReadResult::Loaded ||
      TaskProtection::inspectFormat(stored) !=
          TaskProtection::Format::LegacyPlaintext) {
    // Protected formats are not integrated yet. Never downgrade to plaintext
    // or overwrite an unreadable/corrupt existing file.
    dataIn.recoveryReadOnly = true;
    return false;
  }

  if (!TaskRestore::decodeLegacyTransactional(
          stored, dataIn.tasks,
          [](QDataStream &stream, JobOptions &record) {
            TaskJobCodec::read(stream, record);
          })) {
    dataIn.recoveryReadOnly = true;
    return false;
  }
  return true;
}

bool ListOfJobOptions::PersistToUserData() {
  if (recoveryReadOnly) return false;
  const QString path = GetPersistenceFilePath();
  const bool saved = TaskStorage::saveAtomically(
      path, [this](QDataStream &outstream) {
        for (JobOptions *it : tasks)
          TaskJobCodec::write(outstream, *it);
        return true;
      });
  if (!saved)
    return false; // preserve the old file if serialization or commit failed

  emit tasksListUpdated();
  return true;
}

