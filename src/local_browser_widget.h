#pragma once

#include "pch.h"

// Native local filesystem view for the dual-pane manager. Uses Qt's filesystem
// model, not an artificial rclone remote, so drives and SMB/UNC paths work.
class LocalBrowserWidget : public QWidget {
  Q_OBJECT

public:
  explicit LocalBrowserWidget(const QString &initialDirectory,
                              QWidget *parent = nullptr);

  QString selectedPath() const;
  QString selectedName() const;
  bool selectedIsFolder() const;
  QString currentDirectory() const { return mDirectory; }
  QAbstractItemView *fileView() const { return mFiles; }
  bool dragEntryAt(const QPoint &viewportPos, QString *path,
                   QString *name, bool *folder) const;
  QString dropFolderAt(const QPoint &viewportPos) const;

signals:
  void directoryChanged(const QString &directory);

private:
  QFileSystemModel *mModel = nullptr;
  QTreeView *mFiles = nullptr;
  QComboBox *mDrives = nullptr;
  QLineEdit *mPath = nullptr;
  QToolButton *mUp = nullptr;
  QString mDirectory;

  void goToDirectory(const QString &directory);
  void browseDirectory();
};
