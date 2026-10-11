#include "dual_pane_widget.h"

#include "icon_cache.h"
#include "local_browser_widget.h"
#include "remote_widget.h"
#include "theme.h"
#include "utils.h"

namespace {

// A wider grip makes the existing splitter discoverable and easy to drag.
// Painting from live theme colors also keeps it legible after theme switches.
class FileManagerSplitterHandle final : public QSplitterHandle {
public:
  FileManagerSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
      : QSplitterHandle(orientation, parent) {
    setToolTip(QObject::tr("Drag left or right to resize the file panes"));
    setAccessibleName(QObject::tr("Resize file manager panes"));
  }

protected:
  void enterEvent(QEnterEvent *event) override {
    mHovered = true;
    update();
    QSplitterHandle::enterEvent(event);
  }

  void leaveEvent(QEvent *event) override {
    mHovered = false;
    update();
    QSplitterHandle::leaveEvent(event);
  }

  void paintEvent(QPaintEvent *) override {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF area(rect());
    const QColor divider = Theme::color("divider");
    const QColor accent = Theme::color("accentText");
    const QColor neutral = Theme::color("text3");
    const qreal x = area.center().x();

    // A faint full-height central rule and three clear grab marks.
    QPen linePen(divider, 1);
    p.setPen(linePen);
    p.drawLine(QPointF(x, 4), QPointF(x, height() - 4));
    p.setPen(Qt::NoPen);
    const QColor handleBg = mHovered ? Theme::color("navSel")
                                     : Theme::color("layer");
    p.setBrush(handleBg);
    p.drawRoundedRect(QRectF(x - 5, height() / 2.0 - 29, 10, 58), 5, 5);

    p.setBrush(mHovered ? accent : neutral);
    for (int i = -1; i <= 1; ++i) {
      p.drawRoundedRect(QRectF(x - 1, height() / 2.0 + i * 10 - 3,
                               2, 6), 1, 1);
    }
  }

private:
  bool mHovered = false;
};

class FileManagerSplitter final : public QSplitter {
public:
  explicit FileManagerSplitter(QWidget *parent)
      : QSplitter(Qt::Horizontal, parent) {}

protected:
  QSplitterHandle *createHandle() override {
    return new FileManagerSplitterHandle(orientation(), this);
  }
};

} // namespace


DualPaneWidget::DualPaneWidget(IconCache *icons, QWidget *parent)
    : QWidget(parent), mIcons(icons) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 20, 24, 12);
  layout->setSpacing(10);

  auto *heading = new QLabel(tr("File manager"));
  heading->setObjectName("PageTitle");
  auto *titleRow = new QHBoxLayout;
  titleRow->addWidget(heading);
  titleRow->addStretch(1);
  auto *equalPanes = new QToolButton(this);
  equalPanes->setText(tr("50:50"));
  equalPanes->setToolTip(tr("Make both file panes equally wide"));
  equalPanes->setAccessibleName(tr("Reset file panes to equal widths"));
  titleRow->addWidget(equalPanes);
  layout->addLayout(titleRow);

  auto *info = new QLabel(tr("Browse local drives, NAS shares and cloud remotes side by side. "
                             "Drag the center grip to resize, or drag a file to the other pane to copy. "
                             "Copies require confirmation and never overwrite existing files."));
  info->setObjectName("PageSubtitle");
  info->setWordWrap(true);
  layout->addWidget(info);

  mSplitter = new FileManagerSplitter(this);
  mSplitter->setObjectName("FileManagerSplitter");
  mSplitter->setHandleWidth(14);
  mSplitter->setOpaqueResize(true);
  mSplitter->setChildrenCollapsible(false);
  buildPane(mLeft, tr("LEFT"), "left");
  buildPane(mRight, tr("RIGHT"), "right");
  mSplitter->addWidget(mLeft.body);
  mSplitter->addWidget(mRight.body);
  mSplitter->setStretchFactor(0, 1);
  mSplitter->setStretchFactor(1, 1);

  // Keep the user's preferred pane widths between launches. QSplitter
  // serializes its own sizes and orientation as a QByteArray.
  const QByteArray saved =
      GetSettings()->value("FileManager/SplitterState").toByteArray();
  if (saved.isEmpty() || !mSplitter->restoreState(saved))
    mSplitter->setSizes({500, 500});
  connect(mSplitter, &QSplitter::splitterMoved, this,
          [this](int, int) {
            GetSettings()->setValue("FileManager/SplitterState",
                                    mSplitter->saveState());
          });
  connect(equalPanes, &QToolButton::clicked, this, [this]() {
    const int total = mSplitter->sizes().value(0) +
                      mSplitter->sizes().value(1);
    const int half = qMax(1, total / 2);
    mSplitter->setSizes({half, half});
    GetSettings()->setValue("FileManager/SplitterState",
                            mSplitter->saveState());
  });
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
  // Allow a narrow secondary pane even when its internal browser has wide
  // toolbar size hints; keep a minimum usable width for both panes.
  pane.body->setMinimumWidth(220);
  pane.body->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
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
    registerPaneViews(pane);
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
  registerPaneViews(pane);
  connect(browser, &RemoteWidget::addTransfer, this,
          &DualPaneWidget::addTransfer);
  connect(browser, &RemoteWidget::addMount, this, &DualPaneWidget::addMount);
  connect(browser, &RemoteWidget::addStream, this, &DualPaneWidget::addStream);
  connect(browser, &RemoteWidget::newTabRequested, this,
          [this, remote, type]() { emit openInTab(remote, type); });
  connect(browser, &RemoteWidget::closeRequested, this,
          [&pane]() { pane.chooser->setCurrentIndex(0); });
}

void DualPaneWidget::registerPaneViews(Pane &pane) {
  QList<QAbstractItemView *> views;
  if (pane.localBrowser)
    views.append(pane.localBrowser->fileView());
  else if (pane.browser)
    views = pane.browser->dragViews();
  for (QAbstractItemView *view : views) {
    if (!view) continue;
    // Preserve the original Windows Explorer -> cloud drop handling. Our
    // filter intercepts only a one-time internal application MIME type.
    view->setAcceptDrops(true);
    view->viewport()->setAcceptDrops(true);
    view->viewport()->installEventFilter(this);
    view->setToolTip(tr("Drag a file or folder to the other pane to copy"));
  }
}

const DualPaneWidget::Pane *
DualPaneWidget::paneForViewport(QObject *watched,
                                QAbstractItemView **resolvedView) const {
  const Pane *panes[] = {&mLeft, &mRight};
  for (const Pane *pane : panes) {
    QList<QAbstractItemView *> views;
    if (pane->localBrowser)
      views.append(pane->localBrowser->fileView());
    else if (pane->browser)
      views = pane->browser->dragViews();
    for (QAbstractItemView *view : views) {
      if (view && watched == view->viewport()) {
        if (resolvedView) *resolvedView = view;
        return pane;
      }
    }
  }
  return nullptr;
}

bool DualPaneWidget::eventFilter(QObject *watched, QEvent *event) {
  static const QByteArray mimeType("application/x-cloud-control-pane-copy");
  QAbstractItemView *view = nullptr;
  const Pane *pane = paneForViewport(watched, &view);
  if (!pane || !view) return QWidget::eventFilter(watched, event);

  switch (event->type()) {
  case QEvent::MouseButtonPress: {
    auto *mouse = static_cast<QMouseEvent *>(event);
    mPressedViewport.clear();
    if (mouse->button() == Qt::LeftButton &&
        view->indexAt(mouse->position().toPoint()).isValid()) {
      mPressedViewport = view->viewport();
      mPressedPoint = mouse->position().toPoint();
    }
    break;
  }
  case QEvent::MouseButtonRelease:
    if (mPressedViewport == watched) mPressedViewport.clear();
    break;
  case QEvent::MouseMove: {
    auto *mouse = static_cast<QMouseEvent *>(event);
    if (mPressedViewport != watched ||
        !(mouse->buttons() & Qt::LeftButton) ||
        (mouse->position().toPoint() - mPressedPoint).manhattanLength() <
            QApplication::startDragDistance())
      break;

    mPressedViewport.clear();
    QString path;
    QString name;
    bool folder = false;
    const bool valid = pane->localBrowser
        ? pane->localBrowser->dragEntryAt(mPressedPoint, &path, &name, &folder)
        : pane->browser->dragEntryAt(view, mPressedPoint, &path, &name, &folder);
    if (!valid || path.isEmpty()) break;

    mDraggedFrom = pane->key;
    mDraggedPath = path;
    mDraggedName = name;
    mDraggedFolder = folder;
    mDraggedFromLocal = pane->localBrowser != nullptr;
    mDragToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto *mime = new QMimeData;
    mime->setData(mimeType, mDragToken.toUtf8());
    auto *drag = new QDrag(view);
    drag->setMimeData(mime);
    drag->exec(Qt::CopyAction, Qt::CopyAction);
    drag->deleteLater();
    mDragToken.clear();
    mDraggedFrom.clear();
    mDraggedPath.clear();
    mDraggedName.clear();
    return true;
  }
  case QEvent::DragEnter:
  case QEvent::DragMove:
  case QEvent::Drop: {
    auto *drop = static_cast<QDropEvent *>(event);
    if (mDragToken.isEmpty() || pane->key == mDraggedFrom ||
        !drop->mimeData()->hasFormat(mimeType) ||
        drop->mimeData()->data(mimeType) != mDragToken.toUtf8()) {
      break;
    }

    drop->setDropAction(Qt::CopyAction);
    drop->accept();
    if (event->type() != QEvent::Drop) return true;

    const QString destDirectory = pane->localBrowser
        ? pane->localBrowser->dropFolderAt(drop->position().toPoint())
        : pane->browser->dropFolderAt(view, drop->position().toPoint());
    const QString src = mDraggedPath;
    const QString name = mDraggedName;
    const bool folder = mDraggedFolder;
    const bool sourceIsLocal = mDraggedFromLocal;
    const bool targetIsLocal = pane->localBrowser != nullptr;

    // Finish the Qt drag event before opening the confirmation dialog.
    QTimer::singleShot(0, this,
                       [this, src, name, folder, sourceIsLocal, targetIsLocal,
                        destDirectory]() {
      copyWithConfirmation(src, name, folder, sourceIsLocal, targetIsLocal,
                           destDirectory);
    });
    return true;
  }
  default:
    break;
  }

  return QWidget::eventFilter(watched, event);
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

  copyWithConfirmation(src, name, folder, source.localBrowser != nullptr,
                       target.localBrowser != nullptr, targetDirectory);
}

void DualPaneWidget::copyWithConfirmation(
    const QString &src, const QString &name, bool folder,
    bool sourceIsLocal, bool targetIsLocal, const QString &targetDirectory) {
  if (src.isEmpty() || name.isEmpty() || targetDirectory.isEmpty()) {
    QMessageBox::information(
        this, tr("Select a source"),
        tr("Select a file or folder in the source pane. "
           "The other pane's open folder is the destination."));
    return;
  }
  if (name == "." || name == "..") return;
  const QString dst = targetIsLocal
                          ? QDir::cleanPath(QDir(targetDirectory).filePath(name))
                          : targetDirectory +
                                (targetDirectory.endsWith('/') ||
                                         targetDirectory.endsWith(':')
                                     ? "" : "/") +
                                name;

  // Disallow recursive copies and self-copy. Canonical filesystem paths
  // also reject symlink-based descendants of an existing local folder.
  const auto cmp = sourceIsLocal && targetIsLocal
#ifdef Q_OS_WIN
                       ? Qt::CaseInsensitive
#else
                       ? Qt::CaseSensitive
#endif
                       : Qt::CaseSensitive;
  bool recursive = src.compare(dst, cmp) == 0 ||
                   (folder && dst.startsWith(src + "/", cmp));
  if (folder && sourceIsLocal && targetIsLocal) {
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

  // All copy methods go through the same already-tested rclone job system.
  // --ignore-existing prevents overwriting; never move, delete, or sync.
  const QStringList args = {folder ? "copy" : "copyto",
                            src, dst, "--ignore-existing",
                            "--progress", "--stats=1s"};
  emit addTransfer(tr("Copy %1").arg(name), src, dst, args);
}
