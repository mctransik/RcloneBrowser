#include "local_browser_widget.h"

#include "theme.h"

LocalBrowserWidget::LocalBrowserWidget(const QString &initialDirectory,
                                       QWidget *parent)
    : QWidget(parent) {
  auto *v = new QVBoxLayout(this);
  v->setContentsMargins(5, 5, 5, 5);
  v->setSpacing(7);

  auto *toolbar = new QHBoxLayout;
  toolbar->setSpacing(5);
  auto *localLabel = new QLabel(tr("This PC / local storage"));
  localLabel->setObjectName("PageSubtitle");
  toolbar->addWidget(localLabel);
  toolbar->addStretch();

  mBack = new QToolButton(this);
  mBack->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
  mBack->setToolTip(tr("Back to previous folder (Alt+Left)"));
  mBack->setAccessibleName(tr("Back to previous local folder"));
  mBack->setEnabled(false);
  toolbar->addWidget(mBack);

  mForward = new QToolButton(this);
  mForward->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
  mForward->setToolTip(tr("Forward to next folder (Alt+Right)"));
  mForward->setAccessibleName(tr("Forward to next local folder"));
  mForward->setEnabled(false);
  toolbar->addWidget(mForward);

  mUp = new QToolButton(this);
  mUp->setIcon(Theme::icon("up"));
  mUp->setToolTip(tr("Parent folder (Alt+Up)"));
  mUp->setShortcut(QKeySequence(Qt::ALT | Qt::Key_Up));
  toolbar->addWidget(mUp);

  auto *choose = new QToolButton(this);
  choose->setIcon(Theme::icon("folder"));
  choose->setToolTip(tr("Choose a folder or network share…"));
  choose->setText(tr("Browse…"));
  choose->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  toolbar->addWidget(choose);
  v->addLayout(toolbar);

  auto *location = new QHBoxLayout;
  location->setSpacing(5);
  mDrives = new QComboBox(this);
  mDrives->setAccessibleName(tr("Local drive"));
  mDrives->setToolTip(tr("Choose a local or mapped network drive"));
  mDrives->addItem(tr("Drives"), QString());
  for (const QFileInfo &drive : QDir::drives()) {
    const QString path = QDir::fromNativeSeparators(drive.absoluteFilePath());
    mDrives->addItem(QDir::toNativeSeparators(path), path);
  }
  mDrives->setMinimumWidth(85);
  mDrives->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
  location->addWidget(mDrives);

  mPath = new QLineEdit(this);
  mPath->setPlaceholderText(tr("Folder path, e.g. C:\\ or \\\\NAS\\share"));
  mPath->setAccessibleName(tr("Local folder or network path"));
  mPath->setClearButtonEnabled(false);
  location->addWidget(mPath, 1);
  v->addLayout(location);

  mModel = new QFileSystemModel(this);
  mModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs);
  mModel->setReadOnly(true); // transfers use the explicit copy confirmation
  mModel->setRootPath(QDir::rootPath());

  mFiles = new QTreeView(this);
  mFiles->setObjectName("LocalFiles");
  mFiles->setModel(mModel);
  mFiles->setRootIsDecorated(false);
  mFiles->setItemsExpandable(false);
  mFiles->setAlternatingRowColors(false);
  mFiles->setUniformRowHeights(true);
  mFiles->setSortingEnabled(true);
  mFiles->sortByColumn(0, Qt::AscendingOrder);
  mFiles->setSelectionMode(QAbstractItemView::SingleSelection);
  mFiles->setSelectionBehavior(QAbstractItemView::SelectRows);
  mFiles->setEditTriggers(QAbstractItemView::NoEditTriggers);
  mFiles->setFrameShape(QFrame::NoFrame);
  mFiles->setContextMenuPolicy(Qt::NoContextMenu);
  mFiles->header()->setStretchLastSection(false);
  mFiles->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int i = 1; i < 4; ++i)
    mFiles->header()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
  v->addWidget(mFiles, 1);

  connect(mBack, &QToolButton::clicked, this,
          [this]() { navigateHistory(-1); });
  connect(mForward, &QToolButton::clicked, this,
          [this]() { navigateHistory(1); });

  // Each local pane has its own history. Limit keyboard shortcuts to the
  // focused pane; two local panes must never compete for Alt+Left/Right.
  auto *backShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Left), this);
  backShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(backShortcut, &QShortcut::activated, mBack, &QToolButton::click);

  auto *forwardShortcut = new QShortcut(QKeySequence(Qt::ALT | Qt::Key_Right), this);
  forwardShortcut->setContext(Qt::WidgetWithChildrenShortcut);
  connect(forwardShortcut, &QShortcut::activated, mForward, &QToolButton::click);

  connect(mUp, &QToolButton::clicked, this, [this]() {
    const QDir dir(mDirectory);
    const QString parent = QDir::cleanPath(dir.absoluteFilePath(".."));
    if (parent != mDirectory) goToDirectory(parent);
  });
  connect(choose, &QToolButton::clicked, this,
          &LocalBrowserWidget::browseDirectory);
  connect(mPath, &QLineEdit::returnPressed, this, [this]() {
    goToDirectory(mPath->text());
  });
  connect(mDrives, QOverload<int>::of(&QComboBox::activated),
          this, [this](int index) {
    const QString drive = mDrives->itemData(index).toString();
    if (!drive.isEmpty()) goToDirectory(drive);
  });
  connect(mFiles, &QTreeView::doubleClicked, this,
          [this](const QModelIndex &index) {
    if (mModel->isDir(index)) goToDirectory(mModel->filePath(index));
  });

  QString start = initialDirectory;
  if (!QFileInfo(start).isDir())
    start = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  if (!QFileInfo(start).isDir()) start = QDir::rootPath();
  goToDirectory(start);
}

void LocalBrowserWidget::browseDirectory() {
  const QString path = QFileDialog::getExistingDirectory(
      this, tr("Choose local folder or network share"), mDirectory,
      QFileDialog::ShowDirsOnly);
  if (!path.isEmpty()) goToDirectory(path);
}

void LocalBrowserWidget::navigateHistory(int step) {
  const int next = mHistoryIndex + step;
  if (next < 0 || next >= mHistory.size()) return;
  // A disconnected NAS share may no longer exist. Do not advance history
  // unless navigation has actually succeeded.
  if (goToDirectory(mHistory.at(next), false)) {
    mHistoryIndex = next;
    updateHistoryButtons();
  }
}

void LocalBrowserWidget::updateHistoryButtons() {
  mBack->setEnabled(mHistoryIndex > 0);
  mForward->setEnabled(mHistoryIndex >= 0 &&
                       mHistoryIndex + 1 < mHistory.size());
}

bool LocalBrowserWidget::goToDirectory(const QString &directory,
                                       bool recordHistory) {
  const QString path = QDir::cleanPath(QDir::fromNativeSeparators(directory.trimmed()));
  const QFileInfo location(path);
  if (!location.exists() || !location.isDir()) {
    QMessageBox::warning(this, tr("Folder not available"),
                         tr("This folder is not available:\n%1").arg(directory));
    mPath->setText(QDir::toNativeSeparators(mDirectory));
    return false;
  }
  mDirectory = QDir::fromNativeSeparators(location.absoluteFilePath());
  const QModelIndex root = mModel->setRootPath(mDirectory);
  mFiles->setRootIndex(root);
  mFiles->selectionModel()->clearSelection();
  mFiles->selectionModel()->setCurrentIndex(QModelIndex(),
                                           QItemSelectionModel::NoUpdate);
  mPath->setText(QDir::toNativeSeparators(mDirectory));
  mPath->setToolTip(mPath->text());
  mUp->setEnabled(QDir::cleanPath(QDir(mDirectory).absoluteFilePath("..")) !=
                  QDir::cleanPath(mDirectory));
  QString rootPath = QDir::fromNativeSeparators(QDir(mDirectory).rootPath());
  int driveIndex = 0;
  for (int i = 1; i < mDrives->count(); ++i) {
    const QString drive = mDrives->itemData(i).toString();
    if (mDirectory.startsWith(drive, Qt::CaseInsensitive)) {
      driveIndex = i;
      break;
    }
  }
  Q_UNUSED(rootPath);
  const QSignalBlocker block(mDrives);
  mDrives->setCurrentIndex(driveIndex);

  if (recordHistory) {
    // Navigating after Back drops the old forward branch, as in Explorer.
    // Reopening the current folder is not a new navigation step.
    if (mHistoryIndex < 0 ||
        mHistory.value(mHistoryIndex) != mDirectory) {
      mHistory = mHistory.mid(0, mHistoryIndex + 1);
      mHistory.append(mDirectory);
      if (mHistory.size() > 50) mHistory.removeFirst();
      mHistoryIndex = mHistory.size() - 1;
    }
    updateHistoryButtons();
  }
  emit directoryChanged(mDirectory);
  return true;
}

QString LocalBrowserWidget::selectedPath() const {
  const QModelIndex current = mFiles->selectionModel()->currentIndex();
  if (!current.isValid() || current == mFiles->rootIndex() ||
      !mFiles->selectionModel()->isSelected(current))
    return {};
  return QDir::fromNativeSeparators(mModel->filePath(current));
}

QString LocalBrowserWidget::selectedName() const {
  const QString path = selectedPath();
  return path.isEmpty() ? QString() : QFileInfo(path).fileName();
}

bool LocalBrowserWidget::selectedIsFolder() const {
  const QModelIndex current = mFiles->selectionModel()->currentIndex();
  return !selectedPath().isEmpty() && mModel->isDir(current);
}

bool LocalBrowserWidget::dragEntryAt(const QPoint &viewportPos,
                                      QString *path, QString *name,
                                      bool *folder) const {
  const QModelIndex item = mFiles->indexAt(viewportPos);
  if (!item.isValid()) return false;
  const QString absolute = QDir::fromNativeSeparators(mModel->filePath(item));
  const QFileInfo info(absolute);
  if (!info.exists() || info.fileName().isEmpty()) return false;
  *path = absolute;
  *name = info.fileName();
  *folder = mModel->isDir(item);
  return true;
}

QString LocalBrowserWidget::dropFolderAt(const QPoint &viewportPos) const {
  const QModelIndex hovered = mFiles->indexAt(viewportPos);
  if (hovered.isValid() && mModel->isDir(hovered))
    return QDir::fromNativeSeparators(mModel->filePath(hovered));
  return mDirectory;
}
