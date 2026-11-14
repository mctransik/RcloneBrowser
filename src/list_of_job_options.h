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
  // Conservative UI-only hint: true if a task file is present OR its
  // location is unavailable. Does not assert deserialization success.
  static bool MayHaveExistingTaskFile();

signals:
  void tasksListUpdated();

private:
  static ListOfJobOptions *SavedJobOptions;
  static const QString persistenceFileName;
  static bool RestoreFromUserData(ListOfJobOptions &dataIn);
  static QString GetPersistenceFilePath();
  static QFile *GetPersistenceFile(QIODevice::OpenModeFlag mode);

  QList<JobOptions *> tasks;
  bool PersistToUserData();
};
