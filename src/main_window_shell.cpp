// Windows 11 style application shell for MainWindow: navigation pane on the
// left, content pages on the right. The original pages (remotes, jobs, tasks)
// and their logic are kept; this file only rearranges and restyles them.

#include "main_window.h"
#include "dual_pane_widget.h"
#include "remote_widget.h"
#include "theme.h"
#include "utils.h"

namespace {

enum NavRole {
  KindRole = Qt::UserRole + 1, // "page" or "remote"
  TargetRole,                  // page index or remote name
  TypeRole,                    // remote type
  BadgeRole,                   // number shown on the right (0 = none)
  FilledIconRole               // icon used while selected (Windows 11 style)
};

// Sidebar entry: rounded highlight, gold text and icon when selected,
// optional count badge on the right.
class NavDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;

  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return QSize(200, 42);
  }

  void paint(QPainter *p, const QStyleOptionViewItem &opt,
             const QModelIndex &index) const override {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;
    const QRectF r = QRectF(opt.rect).adjusted(6, 3, -6, -3);
    if (selected || hover) {
      p->setPen(Qt::NoPen);
      p->setBrush(selected ? Theme::color("navSel") : Theme::color("subtleHover"));
      p->drawRoundedRect(r, 10, 10);
    }
    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    if (selected && index.data(FilledIconRole).isValid()) {
      icon = index.data(FilledIconRole).value<QIcon>();
    }
    const QRect iconRect(int(r.left()) + 14, int(r.center().y()) - 11, 22, 22);
    icon.paint(p, iconRect, Qt::AlignCenter, selected ? QIcon::Selected : QIcon::Normal);

    QFont f = Theme::uiFont(1.04);
    if (selected) {
      f.setWeight(QFont::DemiBold);
    }
    p->setFont(f);
    p->setPen(selected ? Theme::color("accentText") : Theme::color("text"));

    const int badge = index.data(BadgeRole).toInt();
    QRectF textRect(iconRect.right() + 14, r.top(), r.right() - iconRect.right() - 24, r.height());
    if (badge > 0) {
      const QString b = QString::number(badge);
      QFont bf = Theme::uiFont(0.85);
      bf.setWeight(QFont::DemiBold);
      const qreal w = qMax<qreal>(22, QFontMetricsF(bf).horizontalAdvance(b) + 12);
      const QRectF pill(r.right() - 12 - w, r.center().y() - 11, w, 22);
      p->setPen(Qt::NoPen);
      p->setBrush(selected ? Theme::color("accentBtn") : Theme::color("tile"));
      p->drawRoundedRect(pill, 11, 11);
      p->setFont(bf);
      p->setPen(selected ? Theme::color("onAccent") : Theme::color("text2"));
      p->drawText(pill, Qt::AlignCenter, b);
      p->setFont(f);
      p->setPen(selected ? Theme::color("accentText") : Theme::color("text"));
      textRect.setRight(pill.left() - 8);
    }
    const QString text = QFontMetrics(f).elidedText(
        index.data(Qt::DisplayRole).toString(), Qt::ElideRight, int(textRect.width()));
    p->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text);
    p->restore();
  }
};

enum Page { HomePage = 0, TransfersPage = 1, TasksPage = 2, BrowserPage = 3,
            DashboardPage = 4, FileManagerPage = 5, SecurityPage = 6 };

QLabel *makeLabel(const QString &text, const char *objectName) {
  auto *l = new QLabel(text);
  l->setObjectName(objectName);
  return l;
}

// Title + subtitle on the left, optional widgets on the right.
QWidget *makePageHeader(const QString &title, const QString &subtitle,
                        QLabel **subtitleOut, const QList<QWidget *> &right) {
  auto *header = new QWidget;
  auto *h = new QHBoxLayout(header);
  h->setContentsMargins(0, 0, 0, 8);
  auto *texts = new QVBoxLayout;
  texts->setSpacing(2);
  texts->addWidget(makeLabel(title, "PageTitle"));
  auto *sub = makeLabel(subtitle, "PageSubtitle");
  sub->setWordWrap(true);
  texts->addWidget(sub);
  if (subtitleOut) {
    *subtitleOut = sub;
  }
  h->addLayout(texts, 1);
  for (QWidget *w : right) {
    h->addWidget(w, 0, Qt::AlignBottom);
  }
  return header;
}

// Home page remote card: icon tile, name, remote type.
enum StorageRole {
  CapacityTotalRole = Qt::UserRole + 11,
  CapacityUsedRole,
  CapacityStatusRole
};

// Actual capacity numbers are supplied by explicit rclone about calls.
// Unavailable data is labelled, never invented.
class RemoteCardDelegate : public QStyledItemDelegate {
public:
  using QStyledItemDelegate::QStyledItemDelegate;
  void setCards(bool cards) { mCards = cards; }
  QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
    return mCards ? QSize(234, 164) : QSize(460, 58);
  }

  void paint(QPainter *p, const QStyleOptionViewItem &opt,
             const QModelIndex &index) const override {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const bool selected = opt.state & QStyle::State_Selected;
    const bool hover = opt.state & QStyle::State_MouseOver;
    const QRectF outer = QRectF(opt.rect).adjusted(5, 5, -5, -5);
    p->setPen(QPen(selected ? Theme::color("accentText") : Theme::color("cardStroke")));
    p->setBrush(hover || selected ? Theme::color("cardHover") : Theme::color("card"));
    p->drawRoundedRect(outer, 8, 8);

    const QString name = index.data(Qt::DisplayRole).toString();
    const QString type = index.data(Qt::UserRole).toString();
    QString usage = index.data(CapacityStatusRole).toString();
    if (usage.isEmpty()) usage = QObject::tr("Not queried");
    const QVariant totalValue = index.data(CapacityTotalRole);
    const QVariant usedValue = index.data(CapacityUsedRole);
    const qint64 total = totalValue.isValid() ? totalValue.toLongLong() : -1;
    const qint64 used = usedValue.isValid() ? usedValue.toLongLong() : -1;
    if (total > 0 && used >= 0) {
      usage = QObject::tr("%1 of %2 used")
                  .arg(QLocale().formattedDataSize(used),
                       QLocale().formattedDataSize(total));
    }

    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    QFont nameFont = Theme::uiFont(1.02);
    nameFont.setWeight(QFont::DemiBold);
    p->setFont(nameFont);
    p->setPen(Theme::color("text"));
    if (mCards) {
      icon.paint(p, QRect(int(outer.left() + 14), int(outer.top() + 13), 38, 38));
      QRectF title(outer.left() + 60, outer.top() + 13, outer.width() - 74, 27);
      p->drawText(title, Qt::AlignVCenter,
                  QFontMetrics(nameFont).elidedText(name, Qt::ElideRight, int(title.width())));
      p->setFont(Theme::uiFont(0.9));
      p->setPen(Theme::color("text2"));
      p->drawText(QRectF(title.left(), title.bottom(), title.width(), 20),
                  Qt::AlignVCenter, type);
      p->drawText(QRectF(outer.left() + 14, outer.bottom() - 64,
                         outer.width() - 28, 23), Qt::AlignVCenter, usage);
      if (total > 0 && used >= 0) {
        const QRectF track(outer.left() + 14, outer.bottom() - 31,
                           outer.width() - 28, 7);
        p->setPen(Qt::NoPen);
        p->setBrush(Theme::color("track"));
        p->drawRoundedRect(track, 3.5, 3.5);
        const qreal fill = qBound(0.0, double(used) / double(total), 1.0);
        p->setBrush(Theme::color("accentBtn"));
        p->drawRoundedRect(QRectF(track.left(), track.top(),
                                  track.width() * fill, track.height()), 3.5, 3.5);
      }
    } else {
      icon.paint(p, QRect(int(outer.left() + 12), int(outer.top() + 10), 30, 30));
      const QRectF title(outer.left() + 54, outer.top() + 7, outer.width() * 0.43, 22);
      p->drawText(title, Qt::AlignVCenter,
                  QFontMetrics(nameFont).elidedText(name, Qt::ElideRight, int(title.width())));
      p->setFont(Theme::uiFont(0.88));
      p->setPen(Theme::color("text2"));
      p->drawText(QRectF(title.left(), title.bottom(), title.width(), 18),
                  Qt::AlignVCenter, type);
      p->drawText(QRectF(outer.left() + outer.width() * 0.54, outer.top() + 10,
                         outer.width() * 0.44, outer.height() - 20),
                  Qt::AlignRight | Qt::AlignVCenter, usage);
    }
    p->restore();
  }
private:
  bool mCards = false;
};

void setPageMargins(QLayout *l) {
  l->setContentsMargins(32, 28, 32, 16);
  l->setSpacing(10);
}

} // namespace

void MainWindow::buildShell() {
  setWindowTitle(tr("Rclone Browser"));
  setMinimumSize(960, 600);
  if (!GetSettings()->contains("MainWindow/geometry")) {
    // roomy first-start size, but never larger than the screen
    QSize size(1360, 860);
    if (const QScreen *sc = screen()) {
      size = size.boundedTo(sc->availableGeometry().size() * 0.9);
    }
    resize(size);
  }

  // The classic menu bar is replaced by the navigation pane footer.
  ui.menuBar->hide();

  // ---------------------------------------------------- navigation pane --
  auto *nav = new QWidget;
  nav->setObjectName("NavPane");
  nav->setFixedWidth(292);
  auto *navLayout = new QVBoxLayout(nav);
  navLayout->setContentsMargins(10, 18, 10, 14);
  navLayout->setSpacing(2);

  // brand
  auto *brand = new QWidget;
  auto *bl = new QHBoxLayout(brand);
  bl->setContentsMargins(12, 0, 2, 14);
  bl->setSpacing(10);
  auto *logo = new QLabel;
  logo->setPixmap(qApp->windowIcon().pixmap(32, 32));
  bl->addWidget(logo);
  auto *appName = new QLabel(tr("Rclone Browser"));
  appName->setObjectName("AppTitle");
  bl->addWidget(appName, 1);
  mThemeButton = new QToolButton;
  mThemeButton->setObjectName("ThemeSwitch");
  mThemeButton->setIconSize(QSize(20, 20));
  mThemeButton->setCursor(Qt::PointingHandCursor);
  QObject::connect(mThemeButton, &QToolButton::clicked, this, [this]() {
    Theme::toggle();
    updateThemeButton();
  });
  bl->addWidget(mThemeButton);
  navLayout->addWidget(brand);

  auto setupList = [&](QListWidget *list, const char *name) {
    list->setObjectName(name);
    list->setItemDelegate(new NavDelegate(list));
    list->setIconSize(QSize(22, 22));
    list->setFrameShape(QFrame::NoFrame);
    list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setMouseTracking(true);
    list->setCursor(Qt::PointingHandCursor);
    list->setUniformItemSizes(true);
    list->setFont(Theme::uiFont());
  };

  // main pages
  mNav = new QListWidget;
  setupList(mNav, "NavList");
  auto addPage = [&](const QString &icon, const QString &text, int page) {
    auto *it = new QListWidgetItem(Theme::icon(icon), text, mNav);
    it->setData(KindRole, "page");
    it->setData(TargetRole, page);
    it->setData(FilledIconRole, Theme::icon(icon + "_filled"));
    return it;
  };
  addPage("home", tr("Dashboard"), DashboardPage);
  addPage("cloud", tr("Storage"), HomePage);
  addPage("folder", tr("File manager"), FileManagerPage);
  mNavTransfers = addPage("transfers", tr("Transfers"), TransfersPage);
  addPage("connected", tr("Security"), SecurityPage);
  addPage("tasks", tr("Saved tasks"), TasksPage);
  mNav->setFixedHeight(6 * 42 + 4);
  mNav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  navLayout->addWidget(mNav);

  // remotes section header: title, count, add button
  mNavRemotesHeader = new QWidget;
  auto *rh = new QHBoxLayout(mNavRemotesHeader);
  rh->setContentsMargins(0, 18, 8, 6);
  rh->setSpacing(8);
  auto *rt = new QLabel(tr("Remotes"));
  rt->setObjectName("NavSection");
  rh->addWidget(rt);
  mNavRemoteCount = new QLabel;
  mNavRemoteCount->setObjectName("NavCount");
  rh->addWidget(mNavRemoteCount);
  rh->addStretch(1);
  auto *sortRemotes = new QToolButton;
  sortRemotes->setObjectName("NavAdd");
  sortRemotes->setIcon(Theme::icon("sort"));
  sortRemotes->setToolTip(tr("Sort remotes"));
  sortRemotes->setPopupMode(QToolButton::InstantPopup);
  {
    auto *menu = new QMenu(sortRemotes);
    auto *group = new QActionGroup(menu);
    const QList<QPair<QString, QString>> modes = {
        {"config", tr("Configuration order")}, {"az", tr("Name A–Z")},
        {"za", tr("Name Z–A")}, {"type", tr("Type")},
        {"custom", tr("Custom (drag to reorder)")}};
    const QString current = GetSettings()->value("Settings/remoteSort", "config").toString();
    for (const auto &m : modes) {
      QAction *a = menu->addAction(m.second);
      a->setCheckable(true);
      a->setChecked(m.first == current);
      group->addAction(a);
      QObject::connect(a, &QAction::triggered, this, [this, id = m.first]() {
        GetSettings()->setValue("Settings/remoteSort", id);
        rebuildNavRemotes();
      });
    }
    sortRemotes->setMenu(menu);
  }
  rh->addWidget(sortRemotes);
  auto *addRemote = new QToolButton;
  addRemote->setObjectName("NavAdd");
  addRemote->setIcon(Theme::icon("add"));
  addRemote->setToolTip(tr("New remote"));
  QObject::connect(addRemote, &QToolButton::clicked, ui.config, &QPushButton::click);
  rh->addWidget(addRemote);
  navLayout->addWidget(mNavRemotesHeader);

  mNavRemotes = new QListWidget;
  setupList(mNavRemotes, "NavRemotes");
  mNavRemotes->setContextMenuPolicy(Qt::CustomContextMenu);
  mNavRemotes->setDefaultDropAction(Qt::MoveAction);
  QObject::connect(mNavRemotes->model(), &QAbstractItemModel::rowsMoved, this, [this]() {
    QStringList order;
    for (int i = 0; i < mNavRemotes->count(); ++i) {
      order << mNavRemotes->item(i)->data(TargetRole).toString();
    }
    GetSettings()->setValue("Settings/remoteOrder", order);
  });
  navLayout->addWidget(mNavRemotes, 1);

  // footer
  auto *divider = new QFrame;
  divider->setObjectName("NavDivider");
  navLayout->addSpacing(6);
  navLayout->addWidget(divider);
  navLayout->addSpacing(6);

  // footer: same look as the menu above, but items run commands
  auto *footer = new QListWidget;
  setupList(footer, "NavFooter");
  footer->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto addAction = [&](const QString &icon, const QString &text, const char *id) {
    auto *it = new QListWidgetItem(Theme::icon(icon), text, footer);
    it->setData(KindRole, "action");
    it->setData(TargetRole, id);
    it->setData(FilledIconRole, Theme::icon(icon + "_filled"));
  };
  addAction("github", tr("GitHub"), "github");
  addAction("settings", tr("Settings"), "settings");
  addAction("update", tr("Update & About"), "help");
  footer->setFixedHeight(3 * 42 + 4);
  navLayout->addWidget(footer);

  auto *helpMenu = new QMenu(this);
  QObject::connect(helpMenu, &QMenu::aboutToShow, this, [=, this]() {
    helpMenu->clear();
    for (QAction *a : ui.menuHelp->actions()) {
      helpMenu->addAction(a);
    }
    helpMenu->addSeparator();
    helpMenu->addAction(ui.quit);
  });
  QObject::connect(footer, &QListWidget::itemClicked, this,
                   [=, this](QListWidgetItem *item) {
                     const QString id = item->data(TargetRole).toString();
                     const QRect r = footer->visualItemRect(item);
                     footer->clearSelection();
                     footer->setCurrentItem(nullptr);
                     if (id == "settings") {
                       ui.preferences->trigger();
                     } else if (id == "github") {
                       QDesktopServices::openUrl(
                           QUrl("https://github.com/Jackpison/RcloneBrowser"));
                     } else {
                       helpMenu->exec(footer->viewport()->mapToGlobal(r.topRight()));
                     }
                   });
  updateThemeButton();

  ui.about->setIcon(Theme::icon("info"));
  ui.quit->setIcon(Theme::icon("quit"));
  ui.preferences->setIcon(Theme::icon("settings"));

  // ------------------------------------------------------ content area --
  auto *content = new QWidget;
  content->setObjectName("ContentArea");
  content->setAttribute(Qt::WA_StyledBackground);
  auto *cl = new QVBoxLayout(content);
  cl->setContentsMargins(1, 1, 0, 0);
  cl->setSpacing(0);

  // Home, Transfers and Tasks are pages of their own; the tab widget holds
  // only the open remotes (Explorer-style tabs).
  mPages = new QStackedWidget;
  mPages->setObjectName("Pages");
  QWidget *pageWidgets[3] = {ui.tabs->widget(0), ui.tabs->widget(1), ui.tabs->widget(2)};
  for (int i = 2; i >= 0; --i) {
    ui.tabs->removeTab(i);
  }
  for (QWidget *w : pageWidgets) {
    mPages->addWidget(w);
  }
  mPages->addWidget(ui.tabs);

  // Operational dashboard. Values come from the loaded remotes, active jobs
  // and detected rclone version, never from fabricated cloud usage numbers.
  {
    auto *dashboard = new QWidget;
    auto *v = new QVBoxLayout(dashboard);
    setPageMargins(v);
    v->addWidget(makePageHeader(
        tr("Dashboard"),
        tr("Your clouds, running transfers and update status at a glance."),
        nullptr, {}));
    auto *metrics = new QGridLayout;
    metrics->setSpacing(12);
    auto addMetric = [&](int row, int column, const QString &title, QLabel **value) {
      auto *card = new QFrame(dashboard);
      card->setProperty("card", true);
      card->setAttribute(Qt::WA_StyledBackground);
      auto *contents = new QVBoxLayout(card);
      contents->setContentsMargins(18, 16, 18, 16);
      auto *label = new QLabel(title);
      label->setObjectName("PageSubtitle");
      contents->addWidget(label);
      *value = new QLabel(tr("Not checked"));
      (*value)->setObjectName("PageTitle");
      contents->addWidget(*value);
      metrics->addWidget(card, row, column);
    };
    addMetric(0, 0, tr("Configured remotes"), &mDashboardRemotes);
    addMetric(0, 1, tr("rclone installation"), &mDashboardRclone);
    addMetric(1, 0, tr("Running jobs"), &mDashboardTransfers);
    mDashboardTransfers->setText("0");
    auto *usage = new QFrame(dashboard);
    usage->setProperty("card", true);
    usage->setAttribute(Qt::WA_StyledBackground);
    auto *usageLayout = new QVBoxLayout(usage);
    usageLayout->setContentsMargins(18, 16, 18, 16);
    usageLayout->addWidget(new QLabel(tr("Storage usage")));
    auto *usageNote = new QLabel(tr("Usage appears only when a provider supplies reliable capacity data. "
                                   "Crypt and underlying remotes must not be counted twice."));
    usageNote->setWordWrap(true);
    usageNote->setObjectName("PageSubtitle");
    usageLayout->addWidget(usageNote);
    metrics->addWidget(usage, 1, 1);
    v->addLayout(metrics);
    auto *quick = new QPushButton(tr("Open file manager"));
    quick->setIcon(Theme::icon("folder"));
    v->addWidget(quick, 0, Qt::AlignLeft);
    connect(quick, &QPushButton::clicked, this,
            [this]() { showPage(FileManagerPage); });
    v->addStretch(1);
    mPages->addWidget(dashboard);
  }

  mDualPane = new DualPaneWidget(&mIcons, mPages);
  connect(mDualPane, &DualPaneWidget::addTransfer, this, &MainWindow::addTransfer);
  connect(mDualPane, &DualPaneWidget::addMount, this, &MainWindow::addMount);
  connect(mDualPane, &DualPaneWidget::addStream, this, &MainWindow::addStream);
  connect(mDualPane, &DualPaneWidget::openInTab, this,
          [this](const QString &name, const QString &type) {
            openRemote(name, type, true);
          });
  mPages->addWidget(mDualPane);

  // Readable security information, with unknown state until real evidence
  // exists. A successful version check is NOT a verified update.
  {
    auto *security = new QWidget;
    auto *v = new QVBoxLayout(security);
    setPageMargins(v);
    v->addWidget(makePageHeader(
        tr("Security center"),
        tr("Software versions, signed updates and verifiable build provenance."),
        nullptr, {}));
    auto section = [&](const QString &title, const QString &body) {
      auto *card = new QFrame(security);
      card->setProperty("card", true);
      card->setAttribute(Qt::WA_StyledBackground);
      auto *inner = new QVBoxLayout(card);
      inner->setContentsMargins(18, 14, 18, 14);
      auto *name = new QLabel(title);
      name->setObjectName("PageTitle");
      inner->addWidget(name);
      auto *detail = new QLabel(body);
      detail->setWordWrap(true);
      detail->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                      Qt::LinksAccessibleByMouse);
      inner->addWidget(detail);
      v->addWidget(card);
      return detail;
    };
    mSecurityRclone = section(tr("rclone version"), tr("Not detected yet"));
    section(tr("Signed rclone updates"),
            tr("The updater uses rclone selfupdate to check PGP-signed release checksums. "
               "A last-successful-verification record is not yet implemented, "
               "so this panel does not claim a completed verification."));
    section(tr("Application build provenance"),
            tr("Release attestations must be verified against the actual downloaded archive. "
               "No local attestation verification has been performed by this interface."));
    auto *check = new QPushButton(tr("Check for rclone updates"));
    connect(check, &QPushButton::clicked, this,
            [this]() { checkRcloneUpdate(true); });
    v->addWidget(check, 0, Qt::AlignLeft);
    v->addStretch(1);
    mPages->addWidget(security);
  }
  cl->addWidget(mPages, 1);

  // Compact persistent transfer indicator; opens the existing full job page.
  mTransferStrip = new QToolButton(content);
  mTransferStrip->setIcon(Theme::icon("transfers"));
  mTransferStrip->setText(tr("Transfers"));
  mTransferStrip->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  mTransferStrip->setCursor(Qt::PointingHandCursor);
  mTransferStrip->hide();
  cl->addWidget(mTransferStrip);
  connect(mTransferStrip, &QToolButton::clicked, this,
          [this]() { showPage(TransfersPage); });

  ui.tabs->setAttribute(Qt::WA_StyledBackground);
  ui.tabs->setDocumentMode(true);
  ui.tabs->setTabsClosable(false); // own close buttons, see openRemote()
  ui.tabs->setMovable(true);
  ui.tabs->setElideMode(Qt::ElideRight);
  ui.tabs->setUsesScrollButtons(true);
  ui.tabs->tabBar()->setExpanding(false);
  ui.tabs->tabBar()->setIconSize(QSize(18, 18));
  ui.tabs->tabBar()->setFont(Theme::uiFont()); // never inherit a stale font

  // "+" right after the last tab, as in File Explorer (Qt only offers a
  // corner position, so it is placed manually; see updateNewTabButton()).
  mNewTab = new QToolButton(ui.tabs);
  mNewTab->setObjectName("NewTab");
  mNewTab->setIcon(Theme::icon("add"));
  mNewTab->setIconSize(QSize(16, 16));
  mNewTab->setFixedSize(32, 32);
  mNewTab->setCursor(Qt::PointingHandCursor);
  mNewTab->setToolTip(tr("New tab (Ctrl+T)"));
  QObject::connect(mNewTab, &QToolButton::clicked, this, [this]() {
    QWidget *w = ui.tabs->currentWidget();
    if (w && w->property("remoteName").isValid()) {
      openRemote(w->property("remoteName").toString(),
                 w->property("remoteType").toString(), true);
    }
  });
  QObject::connect(ui.tabs->tabBar(), &QTabBar::tabMoved, this,
                   [this]() { updateNewTabButton(); });

  auto *central = new QWidget;
  auto *hl = new QHBoxLayout(central);
  hl->setContentsMargins(0, 0, 0, 0);
  hl->setSpacing(0);
  hl->addWidget(nav);
  hl->addWidget(content, 1);
  QWidget *old = takeCentralWidget();
  setCentralWidget(central);
  if (old) {
    old->deleteLater();
  }

  // ---------------------------------------------------------- Home page --
  {
    QWidget *page = mPages->widget(HomePage);
    auto *v = qobject_cast<QVBoxLayout *>(page->layout());
    setPageMargins(v);

    ui.config->setText(tr("New remote"));
    ui.config->setProperty("accent", true);
    ui.config->setToolTip(tr("Add or edit remotes with rclone config"));
    ui.refresh->setText(QString());
    ui.refresh->setIcon(Theme::icon("refresh"));
    ui.refresh->setToolTip(tr("Reload remotes"));
    ui.open->hide();

    QLabel *sub = nullptr;
    auto *refreshUsage = new QToolButton;
    refreshUsage->setText(tr("Get capacity"));
    refreshUsage->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    refreshUsage->setIcon(Theme::icon("refresh"));
    refreshUsage->setToolTip(tr("Ask each remote for capacity using rclone about (manual network requests)."));
    connect(refreshUsage, &QToolButton::clicked, this, &MainWindow::refreshStorageCapacities);
    auto *cards = new QToolButton;
    cards->setIcon(Theme::icon("view_grid"));
    cards->setToolTip(tr("Cards"));
    cards->setCheckable(true);
    auto *list = new QToolButton;
    list->setIcon(Theme::icon("view_list"));
    list->setToolTip(tr("List"));
    list->setCheckable(true);
    v->insertWidget(0, makePageHeader(tr("Storage"),
                                      tr("Capacity appears where supported; no estimates or double counting."),
                                      &sub, {refreshUsage, cards, list, ui.refresh, ui.config}));
    mHomeSubtitle = sub;

    ui.remotes->setMovement(QListView::Static);
    ui.remotes->setResizeMode(QListView::Adjust);
    ui.remotes->setIconSize(QSize(40, 40));
    ui.remotes->setWordWrap(true);
    ui.remotes->setSpacing(0);
    ui.remotes->setFrameShape(QFrame::NoFrame);
    ui.remotes->setSelectionRectVisible(false);
    auto *storageDelegate = new RemoteCardDelegate(ui.remotes);
    ui.remotes->setItemDelegate(storageDelegate);
    auto chooseView = [this, storageDelegate, cards, list](bool grid) {
      ui.remotes->setViewMode(grid ? QListView::IconMode : QListView::ListMode);
      ui.remotes->setWrapping(grid);
      ui.remotes->setUniformItemSizes(true);
      ui.remotes->setGridSize(grid ? QSize(234, 164) : QSize());
      ui.remotes->setIconSize(grid ? QSize(40, 40) : QSize(28, 28));
      storageDelegate->setCards(grid);
      cards->setChecked(grid);
      list->setChecked(!grid);
      ui.remotes->doItemsLayout();
      ui.remotes->viewport()->update();
      GetSettings()->setValue("Storage/view", grid ? "cards" : "list");
    };
    connect(cards, &QToolButton::clicked, this, [chooseView]() { chooseView(true); });
    connect(list, &QToolButton::clicked, this, [chooseView]() { chooseView(false); });
    chooseView(GetSettings()->value("Storage/view", "list").toString() == "cards");
    ui.remotes->setFont(Theme::uiFont());
    ui.remotes->setMouseTracking(true);
    ui.remotes->setCursor(Qt::PointingHandCursor);

    mHomeEmpty = new QLabel(
        tr("<b>No remotes yet</b><br><br>Click <b>New remote</b> to connect a "
           "cloud drive, server or folder with rclone's setup assistant."));
    mHomeEmpty->setAlignment(Qt::AlignCenter);
    mHomeEmpty->setProperty("secondary", true);
    mHomeEmpty->hide();
    v->insertWidget(2, mHomeEmpty, 1);
  }

  // ----------------------------------------------------- Transfers page --
  {
    QWidget *page = mPages->widget(TransfersPage);
    auto *v = qobject_cast<QVBoxLayout *>(page->layout());
    setPageMargins(v);
    v->insertWidget(0, makePageHeader(tr("Transfers"),
                                      tr("Running and finished copies, syncs, "
                                         "mounts and streams."),
                                      nullptr, {}));
    ui.jobsArea->setFrameShape(QFrame::NoFrame);
    ui.jobsArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui.jobs->setSpacing(8);
    ui.noJobsAvailable->setText(tr("No transfers yet.\n\nUploads, downloads, "
                                   "mounts and tasks you run will appear here."));
    ui.noJobsAvailable->setAlignment(Qt::AlignCenter);
    ui.noJobsAvailable->setMinimumHeight(160);
  }

  // --------------------------------------------------------- Tasks page --
  {
    QWidget *page = mPages->widget(TasksPage);
    auto *v = qobject_cast<QVBoxLayout *>(page->layout());
    setPageMargins(v);
    v->insertWidget(0, makePageHeader(tr("Saved tasks"),
                                      tr("Saved transfers you can run again "
                                         "with one click."),
                                      nullptr, {}));
    ui.tasksArea->setFrameShape(QFrame::NoFrame);
    ui.tasksListWidget->setFrameShape(QFrame::NoFrame);
    ui.tasksListWidget->setSpacing(0);
    ui.tasksListWidget->setIconSize(QSize(20, 20));
    ui.tasksListWidget->setFont(Theme::uiFont());

    ui.buttonRunTask->setProperty("accent", true);
    ui.buttonDryrunTask->setIcon(Theme::icon("dryrun"));
    ui.buttonEditTask->setIcon(Theme::icon("edit"));
    ui.buttonDeleteTask->setIcon(Theme::icon("delete"));
  }

  mUploadIcon = Theme::icon("upload");
  mDownloadIcon = Theme::icon("download");

  // ------------------------------------------------------- navigation ----
  auto onNav = [this](QListWidgetItem *item) {
    if (!item || mSyncingNav) {
      return;
    }
    if (item->data(KindRole).toString() == "page") {
      showPage(item->data(TargetRole).toInt());
    } else {
      openRemote(item->data(TargetRole).toString(),
                 item->data(TypeRole).toString());
    }
  };
  QObject::connect(mNav, &QListWidget::itemClicked, this, onNav);
  QObject::connect(mNavRemotes, &QListWidget::itemClicked, this, onNav);
  QObject::connect(mNav, &QListWidget::currentItemChanged, this, onNav);
  QObject::connect(mNavRemotes, &QListWidget::currentItemChanged, this, onNav);
  QObject::connect(ui.tabs, &QTabWidget::currentChanged, this, [this](int) {
    syncNavToCurrentPage();
    updateNewTabButton();
  });
  QObject::connect(mPages, &QStackedWidget::currentChanged, this,
                   [this](int) { syncNavToCurrentPage(); });
  QObject::connect(ui.tabs, &QTabWidget::tabCloseRequested, this,
                   [this](int i) { closeRemoteWidget(ui.tabs->widget(i)); });
  // middle-click closes a tab, as in File Explorer and browsers
  ui.tabs->tabBar()->installEventFilter(this);

  QObject::connect(mNavRemotes, &QWidget::customContextMenuRequested, this,
                   [this](const QPoint &pos) {
                     QListWidgetItem *item = mNavRemotes->itemAt(pos);
                     if (!item || item->data(KindRole).toString() != "remote") {
                       return;
                     }
                     const QString name = item->data(TargetRole).toString();
                     QMenu menu;
                     menu.addAction(Theme::icon("open"), tr("Open"), this, [=, this]() {
                       openRemote(name, item->data(TypeRole).toString());
                     });
                     menu.addAction(Theme::icon("new_tab"), tr("Open in new tab"), this, [=, this]() {
                       openRemote(name, item->data(TypeRole).toString(), true);
                     });
                     QAction *close = menu.addAction(Theme::icon("close"), tr("Close all tabs"), this,
                                                     [=, this]() { closeRemote(name); });
                     close->setEnabled(remoteTab(name) >= 0);
                     menu.addSeparator();
                     menu.addAction(Theme::icon("refresh"), tr("Reload remotes"), this,
                                    &MainWindow::rcloneListRemotes);
                     menu.exec(mNavRemotes->viewport()->mapToGlobal(pos));
                   });

  mNav->setCurrentRow(0);
  showPage(DashboardPage);
}

int MainWindow::remoteTab(const QString &name) const {
  for (int i = 0; i < ui.tabs->count(); ++i) {
    if (ui.tabs->widget(i)->property("remoteName").toString() == name) {
      return i;
    }
  }
  return -1;
}

void MainWindow::openRemote(const QString &name, const QString &type,
                            bool newTab) {
  int index = newTab ? -1 : remoteTab(name);
  if (index < 0) {
    const bool isLocal = type == "local";
    const bool isGoogle = type == "drive";
    auto *remote = new RemoteWidget(&mIcons, name, isLocal, isGoogle, ui.tabs);
    remote->setProperty("remoteName", name);
    remote->setProperty("remoteType", type);
    remote->setRemoteType(type);
    QObject::connect(remote, &RemoteWidget::addMount, this, &MainWindow::addMount);
    QObject::connect(remote, &RemoteWidget::addStream, this, &MainWindow::addStream);
    QObject::connect(remote, &RemoteWidget::addTransfer, this, &MainWindow::addTransfer);
    QObject::connect(remote, &RemoteWidget::closeRequested, this,
                     [this, remote]() { closeRemoteWidget(remote); });
    QObject::connect(remote, &RemoteWidget::newTabRequested, this,
                     [this, name, type]() { openRemote(name, type, true); });
    index = ui.tabs->addTab(remote, Theme::remoteIcon(type), name);
    ui.tabs->setTabToolTip(index, QString("%1 · %2").arg(name, type));

    // Own close button: Qt's built-in one is positioned against the tab's
    // outer margin box and ends up above the text line.
    auto *close = new QToolButton;
    close->setObjectName("TabClose");
    close->setIcon(Theme::hoverIcon("close", Qt::white)); // white on red hover
    close->setIconSize(QSize(14, 14));
    close->setFixedSize(24, 24);
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(tr("Close tab (Ctrl+W)"));
    close->setFocusPolicy(Qt::NoFocus);
    QObject::connect(close, &QToolButton::clicked, this,
                     [this, remote]() { closeRemoteWidget(remote); });
    ui.tabs->tabBar()->setTabButton(index, QTabBar::RightSide, close);
  }
  ui.tabs->setCurrentIndex(index);
  showPage(BrowserPage);
  QTimer::singleShot(0, this, [this]() { updateNewTabButton(); });
}

void MainWindow::updateNewTabButton() {
  if (!mNewTab) {
    return;
  }
  QTabBar *bar = ui.tabs->tabBar();
  if (bar->count() == 0) {
    mNewTab->hide();
    return;
  }
  const QRect last = bar->tabRect(bar->count() - 1);
  QPoint pos = bar->mapTo(ui.tabs, QPoint(last.right() + 6,
                                          last.center().y() - mNewTab->height() / 2));
  // with many tabs (scroll buttons visible) keep it inside the widget
  pos.setX(qMin(pos.x(), ui.tabs->width() - mNewTab->width() - 4));
  mNewTab->move(pos);
  mNewTab->show();
  mNewTab->raise();
}

void MainWindow::showPage(int page) {
  if (!mPages) {
    return;
  }
  if (page == BrowserPage && ui.tabs->count() == 0) {
    page = HomePage;
  }
  mPages->setCurrentIndex(page);
  syncNavToCurrentPage();
}

void MainWindow::closeRemoteWidget(QWidget *w) {
  const int index = ui.tabs->indexOf(w);
  if (index < 0) {
    return;
  }
  ui.tabs->removeTab(index); // Qt selects the neighbouring tab
  w->deleteLater();
  QTimer::singleShot(0, this, [this]() { updateNewTabButton(); });
  if (ui.tabs->count() == 0 && mPages->currentIndex() == BrowserPage) {
    showPage(HomePage);
  }
  syncNavToCurrentPage();
}

void MainWindow::closeRemote(const QString &name) {
  int index;
  while ((index = remoteTab(name)) >= 0) {
    closeRemoteWidget(ui.tabs->widget(index));
  }
}

bool MainWindow::eventFilter(QObject *o, QEvent *e) {
  if (o == ui.tabs->tabBar() &&
      (e->type() == QEvent::Resize || e->type() == QEvent::LayoutRequest ||
       e->type() == QEvent::Move || e->type() == QEvent::Show)) {
    QTimer::singleShot(0, this, [this]() { updateNewTabButton(); });
  }
  if (o == ui.tabs->tabBar() && e->type() == QEvent::MouseButtonRelease) {
    auto *me = static_cast<QMouseEvent *>(e);
    if (me->button() == Qt::MiddleButton) {
      const int i = ui.tabs->tabBar()->tabAt(me->position().toPoint());
      if (i >= 0) {
        closeRemoteWidget(ui.tabs->widget(i));
        return true;
      }
    }
  }
  return QMainWindow::eventFilter(o, e);
}

void MainWindow::syncNavToCurrentPage() {
  if (!mNav) {
    return;
  }
  const int index = mPages->currentIndex();
  QWidget *tab = ui.tabs->currentWidget();
  const QString remote = (index == BrowserPage && tab)
                             ? tab->property("remoteName").toString()
                             : QString();
  mSyncingNav = true;
  QListWidget *active = remote.isEmpty() ? mNav : mNavRemotes;
  QListWidget *other = remote.isEmpty() ? mNavRemotes : mNav;
  other->clearSelection();
  other->setCurrentItem(nullptr);
  for (int i = 0; i < active->count(); ++i) {
    QListWidgetItem *it = active->item(i);
    const bool match = remote.isEmpty() ? it->data(TargetRole).toInt() == index
                                        : it->data(TargetRole).toString() == remote;
    if (match) {
      active->setCurrentItem(it);
      break;
    }
  }
  mSyncingNav = false;
}

void MainWindow::rebuildNavRemotes() {
  if (!mNavRemotes) {
    return;
  }
  mSyncingNav = true;
  mNavRemotes->clear();
  mSyncingNav = false;
  const int n = ui.remotes->count();
  QList<QListWidgetItem *> sources;
  for (int i = 0; i < n; ++i) {
    sources << ui.remotes->item(i);
  }
  const QString sortMode = GetSettings()->value("Settings/remoteSort", "config").toString();
  QCollator collator;
  collator.setNumericMode(true);
  collator.setCaseSensitivity(Qt::CaseInsensitive);
  if (sortMode == "az" || sortMode == "za") {
    std::stable_sort(sources.begin(), sources.end(), [&](auto *a, auto *b) {
      const int c = collator.compare(a->text(), b->text());
      return sortMode == "az" ? c < 0 : c > 0;
    });
  } else if (sortMode == "type") {
    std::stable_sort(sources.begin(), sources.end(), [&](auto *a, auto *b) {
      const int c = collator.compare(a->data(Qt::UserRole).toString(),
                                     b->data(Qt::UserRole).toString());
      return c != 0 ? c < 0 : collator.compare(a->text(), b->text()) < 0;
    });
  } else if (sortMode == "custom") {
    const QStringList order = GetSettings()->value("Settings/remoteOrder").toStringList();
    std::stable_sort(sources.begin(), sources.end(), [&](auto *a, auto *b) {
      auto rank = [&](QListWidgetItem *x) {
        const int i = order.indexOf(x->text());
        return i < 0 ? int(order.size()) : i; // new remotes go last
      };
      return rank(a) < rank(b);
    });
  }
  mNavRemotes->setDragDropMode(sortMode == "custom" ? QAbstractItemView::InternalMove
                                                    : QAbstractItemView::NoDragDrop);
  for (QListWidgetItem *src : sources) {
    auto *it = new QListWidgetItem(src->icon(), src->text(), mNavRemotes);
    it->setData(KindRole, "remote");
    it->setData(TargetRole, src->text());
    it->setData(TypeRole, src->data(Qt::UserRole));
    it->setToolTip(src->text() + " · " + src->data(Qt::UserRole).toString());
  }
  if (mDashboardRemotes) mDashboardRemotes->setText(QString::number(n));
  if (mDualPane) {
    QList<QPair<QString, QString>> available;
    for (QListWidgetItem *src : sources) {
      available.append(qMakePair(src->text(), src->data(Qt::UserRole).toString()));
    }
    mDualPane->setRemotes(available);
  }
  mNavRemoteCount->setText(QString::number(n));
  mNavRemoteCount->setVisible(n > 0);
  ui.remotes->setVisible(n > 0);
  mHomeEmpty->setVisible(n == 0);
  mHomeSubtitle->setText(
      n == 0 ? tr("Your cloud storage, as configured in rclone.")
             : (n == 1 ? tr("1 remote configured in rclone. Double-click to browse.")
                       : tr("%1 remotes configured in rclone. Double-click to "
                            "browse.").arg(n)));
  syncNavToCurrentPage();
}

void MainWindow::updateThemeButton() {
  if (mThemeButton) {
    mThemeButton->setIcon(Theme::icon(Theme::isDark() ? "sun" : "moon"));
    mThemeButton->setToolTip(Theme::isDark() ? tr("Switch to light mode")
                                             : tr("Switch to dark mode"));
  }
  if (mNav) {
    mNav->viewport()->update();
    mNavRemotes->viewport()->update();
  }
}

void MainWindow::setJobsTabText(const QString &text) {
  if (mDashboardTransfers) {
    mDashboardTransfers->setText(QString::number(mJobCount));
  }
  if (mTransferStrip) {
    mTransferStrip->setVisible(mJobCount > 0);
    mTransferStrip->setText(tr("%1 active transfer(s) — show details").arg(mJobCount));
  }
  if (mNavTransfers) {
    static const QRegularExpression rx(R"(\((\d+)\))");
    const auto m = rx.match(text);
    mNavTransfers->setData(BadgeRole, m.hasMatch() ? m.captured(1).toInt() : 0);
  }
}


void MainWindow::refreshStorageCapacities() {
  // Explicitly user-initiated queries: rclone about isn't supported by all
  // backends and may generate billable API traffic for object stores.
  const QString rclone = GetRclone();
  if (rclone.isEmpty()) return;
  for (int i = 0; i < ui.remotes->count(); ++i) {
    auto *item = ui.remotes->item(i);
    const QString name = item->text();
    item->setData(CapacityTotalRole, QVariant());
    item->setData(CapacityUsedRole, QVariant());
    item->setData(CapacityStatusRole, tr("Checking…"));

    auto *p = new QProcess(this);
    UseRclonePassword(p);
    p->setProgram(rclone);
    p->setArguments(QStringList() << "about" << name + ":" << "--json"
                                   << GetRcloneConf() << "--ask-password=false");
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p, name](int code, QProcess::ExitStatus status) {
      const QByteArray output = p->readAllStandardOutput();
      QJsonParseError error;
      const QJsonDocument doc = QJsonDocument::fromJson(output, &error);
      for (int i = 0; i < ui.remotes->count(); ++i) {
        auto *item = ui.remotes->item(i);
        if (item->text() != name) continue;
        if (status == QProcess::NormalExit && code == 0 &&
            error.error == QJsonParseError::NoError && doc.isObject()) {
          const QJsonObject obj = doc.object();
          const QJsonValue total = obj.value("total");
          const QJsonValue used = obj.value("used");
          if (total.isDouble() && used.isDouble() &&
              total.toDouble() > 0 && used.toDouble() >= 0) {
            item->setData(CapacityTotalRole, qint64(total.toDouble()));
            item->setData(CapacityUsedRole, qint64(used.toDouble()));
            item->setData(CapacityStatusRole, QString());
          } else {
            item->setData(CapacityStatusRole, tr("Capacity not available"));
          }
        } else {
          item->setData(CapacityStatusRole, tr("Not supported / unavailable"));
        }
        break;
      }
      p->deleteLater();
    });
    connect(p, &QProcess::errorOccurred, this,
            [this, p, name](QProcess::ProcessError) {
      if (p->state() != QProcess::NotRunning) return;
      for (int i = 0; i < ui.remotes->count(); ++i) {
        auto *item = ui.remotes->item(i);
        if (item->text() == name) {
          item->setData(CapacityStatusRole, tr("Query failed"));
          break;
        }
      }
      p->deleteLater();
    });
    QTimer::singleShot(25000, p, [p]() {
      if (p->state() != QProcess::NotRunning) p->kill();
    });
    p->start();
  }
}
