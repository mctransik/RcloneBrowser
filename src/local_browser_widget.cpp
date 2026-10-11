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

void LocalBrowserWidget::goToDirectory(const QString &directory) {
  const QString path = QDir::cleanPath(QDir::fromNativeSeparators(directory.trimmed()));
  const QFileInfo location(path);
  if (!location.exists() || !location.isDir()) {
    QMessageBox::warning(this, tr("Folder not available"),
                         tr("This folder is not available:\n%1").arg(directory));
    mPath->setText(QDir::toNativeSeparators(mDirectory));
    return;
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
  emit directoryChanged(mDirectory);
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
