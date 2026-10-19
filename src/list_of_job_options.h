#pragma once
#include "job_options.h"
#include <qfile.h>

class ListOfJobOptions : public QObject {
  Q_OBJECT

protected:
  ~ListOfJobOptions() = default;
  ListOfJobOptions();

public:
  static ListOfJobOptions *getInstance();
  // True only if tasks.bin was successfully updated.
  // On failure, a newly added item or a removed item is rolled back.
  // Editing callers must restore the previous JobOptions fields themselves.
  bool Persist(JobOptions *jo);
  bool Forget(JobOptions *jo);
  QList<JobOptions *> &getTasks() { return tasks; }
  // Fail closed after an unreadable or unsupported saved-task file.
  bool isRecoveryReadOnly() const { return recoveryReadOnly; }

signals:
  void tasksListUpdated();

private:
  static ListOfJobOptions *SavedJobOptions;
  static const QString persistenceFileName;
  static bool RestoreFromUserData(ListOfJobOptions &dataIn);
  static QString GetPersistenceFilePath();
  static QFile *GetPersistenceFile(QIODevice::OpenModeFlag mode);

  QList<JobOptions *> tasks;
  bool recoveryReadOnly = false;
  bool PersistToUserData();
};
