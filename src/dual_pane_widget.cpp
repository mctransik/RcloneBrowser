#include "dual_pane_widget.h"

#include "icon_cache.h"
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

  auto *info = new QLabel(tr("Browse two locations side by side. Choose a remote in each pane. "
                             "Copying is optional and never overwrites existing destination files."));
  info->setObjectName("PageSubtitle");
  info->setWordWrap(true);
  layout->addWidget(info);

  mSplitter = new QSplitter(Qt::Horizontal, this);
  mSplitter->setChildrenCollapsible(false);
  buildPane(mLeft, tr("SOURCE / LEFT"));
  buildPane(mRight, tr("DESTINATION / RIGHT"));
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

void DualPaneWidget::buildPane(Pane &pane, const QString &caption) {
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
  pane.chooser->setAccessibleName(tr("Remote selection"));
  head->addWidget(pane.chooser, 1);
  vertical->addLayout(head);

  pane.layout = new QVBoxLayout;
  pane.layout->setContentsMargins(0, 0, 0, 0);
  vertical->addLayout(pane.layout, 1);
  connect(pane.chooser, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, [this, &pane](int) { showSelectedRemote(pane); });
}

void DualPaneWidget::setRemotes(const QList<QPair<QString, QString>> &remotes) {
  auto reset = [&](Pane &pane) {
    const QString previous = pane.chooser->currentData(Qt::UserRole).toString();
    QSignalBlocker blocked(pane.chooser);
    pane.chooser->clear();
    pane.chooser->addItem(tr("Choose remote…"), QString());
    for (const auto &r : remotes) {
      pane.chooser->addItem(Theme::remoteIcon(r.second), r.first, r.first);
      pane.chooser->setItemData(pane.chooser->count() - 1, r.second, Qt::UserRole + 1);
    }
    const int index = pane.chooser->findData(previous, Qt::UserRole);
    pane.chooser->setCurrentIndex(index < 0 ? 0 : index);
    // Existing widget can remain if the same remote is still selected.
    if (pane.browser && (index <= 0 || pane.browser->property("remoteName").toString() != previous)) {
      delete pane.browser;
      pane.browser = nullptr;
    }
  };
  reset(mLeft);
  reset(mRight);
  if (!mLeft.browser) showSelectedRemote(mLeft);
  if (!mRight.browser) showSelectedRemote(mRight);
}

void DualPaneWidget::showSelectedRemote(Pane &pane) {
  if (pane.browser) {
    delete pane.browser;
    pane.browser = nullptr;
  }
  // A container placeholder avoids additional rclone calls before selection.
  while (QLayoutItem *item = pane.layout->takeAt(0)) {
    if (item->widget()) delete item->widget();
    delete item;
  }
  const QString remote = pane.chooser->currentData(Qt::UserRole).toString();
  if (remote.isEmpty()) {
    auto *empty = new QLabel(tr("Select a remote above to browse its files."));
    empty->setAlignment(Qt::AlignCenter);
    empty->setWordWrap(true);
    empty->setProperty("secondary", true);
    pane.layout->addWidget(empty, 1);
    return;
  }
  const QString type = pane.chooser->currentData(Qt::UserRole + 1).toString();
  auto *browser = new RemoteWidget(mIcons, remote, type == "local",
                                   type == "drive", pane.body);
  browser->setRemoteType(type);
  browser->setProperty("remoteName", remote);
  pane.browser = browser;
  pane.layout->addWidget(browser, 1);
  connect(browser, &RemoteWidget::addTransfer, this, &DualPaneWidget::addTransfer);
  connect(browser, &RemoteWidget::addMount, this, &DualPaneWidget::addMount);
  connect(browser, &RemoteWidget::addStream, this, &DualPaneWidget::addStream);
  connect(browser, &RemoteWidget::newTabRequested, this,
          [this, remote, type]() { emit openInTab(remote, type); });
  connect(browser, &RemoteWidget::closeRequested, this,
          [&pane]() { pane.chooser->setCurrentIndex(0); });
}

void DualPaneWidget::copyBetween(bool leftToRight) {
  RemoteWidget *source = leftToRight ? mLeft.browser : mRight.browser;
  RemoteWidget *target = leftToRight ? mRight.browser : mLeft.browser;
  if (!source || !target) {
    QMessageBox::information(this, tr("Select remotes"),
                             tr("Choose a remote in both panes first."));
    return;
  }
  const QString src = source->selectedRemotePath();
  const QString targetDir = target->currentRemoteFolder();
  if (src.isEmpty() || targetDir.isEmpty()) {
    QMessageBox::information(this, tr("Select a file or folder"),
                             tr("Select a file or subfolder in the source pane."));
    return;
  }
  const QString name = source->selectedEntryName();
  if (name.isEmpty() || name == "." || name == "..") return;
  const bool folder = source->selectedEntryIsFolder();
  const QString dst = targetDir + (targetDir.endsWith('/') || targetDir.endsWith(':') ? "" : "/") + name;
  if (src == dst || (folder && dst.startsWith(src + "/"))) {
    QMessageBox::warning(this, tr("Invalid destination"),
                         tr("A folder cannot be copied into itself or one of its subfolders."));
    return;
  }
  // Only copy. Never sync, delete, move or overwrite existing files.
  const auto reply = QMessageBox::question(
      this, tr("Confirm cloud copy"),
      tr("Copy %1?\n\nFrom: %2\nTo: %3\n\nExisting destination files "
         "will not be overwritten.").arg(folder ? tr("folder") : tr("file"), src, dst),
      QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
  if (reply != QMessageBox::Yes) return;

  const QStringList args = {folder ? "copy" : "copyto", src, dst,
                            "--ignore-existing", "--progress", "--stats=1s"};
  emit addTransfer(tr("Copy %1").arg(name), src, dst, args);
}
