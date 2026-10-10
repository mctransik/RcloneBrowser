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
  static constexpr const char *kLocalLocation = "@local";
  void buildPane(Pane &pane, const QString &caption,
                 const QString &key);
  void showSelectedRemote(Pane &pane);
  void copyBetween(bool leftToRight);
};
