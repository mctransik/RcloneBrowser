#include "dual_pane_widget.h"

#include "icon_cache.h"
#include "local_browser_widget.h"
#include "remote_widget.h"
#include "theme.h"
#include "utils.h"

DualPaneWidget::DualPaneWidget(IconCache *icons, QWidget *parent)
    : QWidget(parent), mIcons(icons) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 20, 24, 12);
  layout->setSpacing(10);

  auto *heading = new QLabel(tr("File manager"));
  heading->setObjectName("PageTitle");
  layout->addWidget(heading);

  auto *info = new QLabel(tr("Browse local drives, NAS shares and cloud remotes side by side. "
                             "Copying is optional and never overwrites existing destination files."));
  info->setObjectName("PageSubtitle");
  info->setWordWrap(true);
  layout->addWidget(info);

  mSplitter = new QSplitter(Qt::Horizontal, this);
  mSplitter->setChildrenCollapsible(false);
  buildPane(mLeft, tr("LEFT"), "left");
  buildPane(mRight, tr("RIGHT"), "right");
  mSplitter->addWidget(mLeft.body);
  mSplitter->addWidget(mRight.body);
  mSplitter->setStretchFactor(0, 1);
  mSplitter->setStretchFactor(1, 1);
  layout->addWidget(mSplitter, 1);

  auto *actions = new QHBoxLayout;
  auto *leftToRight = new QPushButton(tr("Copy selected to right  →"));
  auto *rightToLeft = new QPushButton(tr("←  Copy selected to left"));
  leftToRight->setIcon(Theme::icon("copy"));
  rightToLeft->setIcon(Theme::icon("copy"));
  leftToRight->setToolTip(tr("Copy the selected file or folder to the other pane; never overwrite."));
  rightToLeft->setToolTip(leftToRight->toolTip());
  actions->addStretch();
  actions->addWidget(rightToLeft);
  actions->addWidget(leftToRight);
  actions->addStretch();
  layout->addLayout(actions);

  connect(leftToRight, &QPushButton::clicked, this,
          [this]() { copyBetween(true); });
  connect(rightToLeft, &QPushButton::clicked, this,
          [this]() { copyBetween(false); });
}

void DualPaneWidget::buildPane(Pane &pane, const QString &caption,
                               const QString &key) {
  pane.key = key;
  pane.body = new QWidget(this);
  pane.body->setObjectName("SplitPane");
  auto *vertical = new QVBoxLayout(pane.body);
  vertical->setContentsMargins(4, 4, 4, 4);
  vertical->setSpacing(6);

  auto *head = new QHBoxLayout;
  auto *label = new QLabel(caption);
  label->setObjectName("PageSubtitle");
  head->addWidget(label);
  pane.chooser = new QComboBox(pane.body);
  pane.chooser->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  pane.chooser->setAccessibleName(tr("Storage location"));
  pane.chooser->setToolTip(tr("Select this PC, a local folder or an rclone remote"));
  head->addWidget(pane.chooser, 1);
  vertical->addLayout(head);

  pane.layout = new QVBoxLayout;
  pane.layout->setContentsMargins(0, 0, 0, 0);
  vertical->addLayout(pane.layout, 1);

  connect(pane.chooser, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this, &pane](int) {
    GetSettings()->setValue("FileManager/" + pane.key + "Location",
                            pane.chooser->currentData(Qt::UserRole));
    showSelectedRemote(pane);
  });
}

void DualPaneWidget::setRemotes(const QList<QPair<QString, QString>> &remotes) {
  auto reset = [&](Pane &pane) {
    const QString before = pane.chooser->currentData(Qt::UserRole).toString();
    const QString remembered = GetSettings()
                                   ->value("FileManager/" + pane.key + "Location")
                                   .toString();
    const QString wanted = before.isEmpty() ? remembered : before;
    const QSignalBlocker blocked(pane.chooser);
    pane.chooser->clear();
    pane.chooser->addItem(tr("Choose a location…"), QString());
    // A first-class local browser is available even without rclone remotes.
    pane.chooser->addItem(Theme::icon("folder"),
                          tr("This PC / local storage"), kLocalLocation);
    for (const auto &remote : remotes) {
      pane.chooser->addItem(Theme::remoteIcon(remote.second),
                            remote.first, remote.first);
      pane.chooser->setItemData(pane.chooser->count() - 1, remote.second,
                                Qt::UserRole + 1);
    }
    int index = pane.chooser->findData(wanted, Qt::UserRole);
    if (index <= 0 && wanted.isEmpty()) {
      index = pane.key == "left" ? 1 : (remotes.isEmpty() ? 0 : 2);
    }
    pane.chooser->setCurrentIndex(index < 0 ? 0 : index);
    const QString after = pane.chooser->currentData(Qt::UserRole).toString();
    const bool sameLocal = after == kLocalLocation && pane.localBrowser;
    const bool sameRemote = !after.isEmpty() && after != kLocalLocation &&
                            pane.browser &&
                            pane.browser->property("remoteName").toString() == after;
    if (!sameLocal && !sameRemote) showSelectedRemote(pane);
  };
  reset(mLeft);
  reset(mRight);
}

void DualPaneWidget::showSelectedRemote(Pane &pane) {
  // Qt removes widgets from their layouts when they are deleted. Clear all
  // old content exactly once before switching between local and remote.
  while (QLayoutItem *item = pane.layout->takeAt(0)) {
    delete item->widget();
    delete item;
  }
  pane.browser = nullptr;
  pane.localBrowser = nullptr;

  const QString remote = pane.chooser->currentData(Qt::UserRole).toString();
  if (remote.isEmpty()) {
    auto *empty = new QLabel(tr("Select local storage or a cloud remote above."));
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    empty->setProperty("secondary", true);
    pane.layout->addWidget(empty, 1);
    return;
  }

  if (remote == kLocalLocation) {
    const QString saved = GetSettings()
                              ->value("FileManager/" + pane.key + "LocalDirectory")
                              .toString();
    auto *local = new LocalBrowserWidget(saved, pane.body);
    pane.localBrowser = local;
    pane.layout->addWidget(local, 1);
    connect(local, &LocalBrowserWidget::directoryChanged,
            this, [key = pane.key](const QString &path) {
      GetSettings()->setValue("FileManager/" + key + "LocalDirectory", path);
    });
    return;
  }

  const QString type = pane.chooser->currentData(Qt::UserRole + 1).toString();
  auto *browser = new RemoteWidget(mIcons, remote, type == "local",
                                   type == "drive", pane.body);
  browser->setRemoteType(type);
  browser->setProperty("remoteName", remote);
  pane.browser = browser;
  pane.layout->addWidget(browser, 1);
  connect(browser, &RemoteWidget::addTransfer, this,
          &DualPaneWidget::addTransfer);
  connect(browser, &RemoteWidget::addMount, this, &DualPaneWidget::addMount);
  connect(browser, &RemoteWidget::addStream, this, &DualPaneWidget::addStream);
  connect(browser, &RemoteWidget::newTabRequested, this,
          [this, remote, type]() { emit openInTab(remote, type); });
  connect(browser, &RemoteWidget::closeRequested, this,
          [&pane]() { pane.chooser->setCurrentIndex(0); });
}

void DualPaneWidget::copyBetween(bool leftToRight) {
  const Pane &source = leftToRight ? mLeft : mRight;
  const Pane &target = leftToRight ? mRight : mLeft;
  if ((!source.browser && !source.localBrowser) ||
      (!target.browser && !target.localBrowser)) {
    QMessageBox::information(this, tr("Choose storage locations"),
                             tr("Choose a storage location in both panes first."));
    return;
  }

  const QString src = source.localBrowser
                          ? source.localBrowser->selectedPath()
                          : source.browser->selectedRemotePath();
  const QString name = source.localBrowser
                           ? source.localBrowser->selectedName()
                           : source.browser->selectedEntryName();
  const bool folder = source.localBrowser
                          ? source.localBrowser->selectedIsFolder()
                          : source.browser->selectedEntryIsFolder();
  const QString targetDirectory = target.localBrowser
                                      ? target.localBrowser->currentDirectory()
                                      : target.browser->currentRemoteFolder();

  if (src.isEmpty() || name.isEmpty() || targetDirectory.isEmpty()) {
    QMessageBox::information(
        this, tr("Select a source"),
        tr("Select a file or folder in the source pane. "
           "The other pane's open folder is the destination."));
    return;
  }
  if (name == "." || name == "..") return;
  const QString dst = target.localBrowser
                          ? QDir::cleanPath(QDir(targetDirectory).filePath(name))
                          : targetDirectory +
                                (targetDirectory.endsWith('/') ||
                                         targetDirectory.endsWith(':')
                                     ? "" : "/") +
                                name;

  // Disallow recursive copies and self-copy. Canonical filesystem paths
  // also reject symlink-based descendants of an existing local folder.
  const auto cmp = source.localBrowser && target.localBrowser
#ifdef Q_OS_WIN
                       ? Qt::CaseInsensitive
#else
                       ? Qt::CaseSensitive
#endif
                       : Qt::CaseSensitive;
  bool recursive = src.compare(dst, cmp) == 0 ||
                   (folder && dst.startsWith(src + "/", cmp));
  if (folder && source.localBrowser && target.localBrowser) {
    const QString from = QFileInfo(src).canonicalFilePath();
    const QString into = QFileInfo(targetDirectory).canonicalFilePath();
    if (!from.isEmpty() && !into.isEmpty()) {
      recursive |= into.compare(from, cmp) == 0 ||
                   into.startsWith(from + "/", cmp);
    }
  }
  if (recursive) {
    QMessageBox::warning(
        this, tr("Invalid destination"),
        tr("A file or folder cannot be copied onto itself, and a folder "
           "cannot be copied into one of its own subfolders."));
    return;
  }

  const auto reply = QMessageBox::question(
      this, tr("Confirm copy"),
      tr("Copy %1?\n\nFrom: %2\nTo: %3\n\nExisting destination files "
         "will not be overwritten.")
          .arg(folder ? tr("folder") : tr("file"), src, dst),
      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
  if (reply != QMessageBox::Yes) return;

  // A single transfer path works for local -> remote (upload), remote ->
  // local (download), remote -> remote and local -> local. Never sync/move.
  // --ignore-existing provides the non-overwrite default.
  const QStringList args = {folder ? "copy" : "copyto",
                            src, dst, "--ignore-existing",
                            "--progress", "--stats=1s"};
  emit addTransfer(tr("Copy %1").arg(name), src, dst, args);
}
