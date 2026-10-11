#pragma once

#include "pch.h"

class IconCache;
class RemoteWidget;
class LocalBrowserWidget;

// Independent two-pane workspace. Reuses the existing RemoteWidget (and its
// rclone-backed models) rather than implementing a second file browser.
class DualPaneWidget : public QWidget {
  Q_OBJECT

public:
  explicit DualPaneWidget(IconCache *icons, QWidget *parent = nullptr);
  void setRemotes(const QList<QPair<QString, QString>> &remotes);

signals:
  void addTransfer(const QString &message, const QString &source,
                   const QString &dest, const QStringList &args);
  void addMount(const QString &remote, const QString &folder);
  void addStream(const QString &remote, const QString &stream);
  void openInTab(const QString &remote, const QString &type);

private:
  struct Pane {
    QWidget *body = nullptr;
    QComboBox *chooser = nullptr;
    QVBoxLayout *layout = nullptr;
    RemoteWidget *browser = nullptr;
    LocalBrowserWidget *localBrowser = nullptr;
    QString key;
  };
  IconCache *mIcons;
  Pane mLeft;
  Pane mRight;
  QSplitter *mSplitter = nullptr;

  // Drag state belongs to this workspace. The custom MIME payload only
  // contains a one-time token, never credentials or rclone configuration.
  QPointer<QWidget> mPressedViewport;
  QPoint mPressedPoint;
  QString mDragToken;
  QString mDraggedFrom;
  QString mDraggedPath;
  QString mDraggedName;
  bool mDraggedFolder = false;
  bool mDraggedFromLocal = false;
  static constexpr const char *kLocalLocation = "@local";
  void buildPane(Pane &pane, const QString &caption,
                 const QString &key);
  void showSelectedRemote(Pane &pane);
  void copyBetween(bool leftToRight);
  void copyWithConfirmation(const QString &sourcePath,
                            const QString &entryName, bool isFolder,
                            bool sourceIsLocal, bool targetIsLocal,
                            const QString &targetDirectory);
  void registerPaneViews(Pane &pane);
  const Pane *paneForViewport(QObject *watched,
                              QAbstractItemView **view = nullptr) const;
  bool eventFilter(QObject *watched, QEvent *event) override;
};
