#include "rclone_updater.h"
#include "utils.h"

namespace {

QString exeName() {
#ifdef Q_OS_WIN
  return "rclone.exe";
#else
  return "rclone";
#endif
}

const char *kUserAgent = "RcloneBrowser/" RCLONE_BROWSER_VERSION;

} // namespace

RcloneUpdater::RcloneUpdater(QObject *parent)
    : QObject(parent), mNet(new QNetworkAccessManager(this)) {
  mSources = {
      {"https://downloads.rclone.org/version.txt",
       "https://downloads.rclone.org/%1"},
      {QString(), "https://github.com/rclone/rclone/releases/download/%1"},
  };

  // Honour the proxy configured in Rclone Browser's preferences; otherwise
  // use the Windows/system proxy settings.
  auto settings = GetSettings();
  QUrl proxyUrl(settings->value("Settings/https_proxy").toString());
  if (settings->value("Settings/useProxy").toBool() && proxyUrl.isValid() &&
      !proxyUrl.host().isEmpty()) {
    mNet->setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, proxyUrl.host(),
                                 quint16(proxyUrl.port(8080)),
                                 proxyUrl.userName(), proxyUrl.password()));
  } else {
    QNetworkProxyFactory::setUseSystemConfiguration(true);
  }
}

QString RcloneUpdater::managedPath() {
  QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (IsPortableMode()) {
#ifdef Q_OS_WIN
    base = qApp->applicationDirPath();
#else
    // An AppImage is read-only: keep rclone next to the .AppImage file.
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty()) {
      base = QFileInfo(appImage).absolutePath();
    }
#endif
  }
  return QDir(base).filePath("rclone/" + exeName());
}

QString RcloneUpdater::bundledPath() {
#ifdef Q_OS_WIN
  return QString(); // the Windows zip ships rclone in the managed location
#else
  // Inside an AppImage, rclone is bundled next to the app binary. Its path
  // changes on every start (temporary mount), so it is never saved.
  const QString appDir = qEnvironmentVariable("APPDIR");
  if (appDir.isEmpty()) {
    return QString();
  }
  const QString path = QDir(appDir).filePath("usr/bin/rclone");
  return QFileInfo(path).isExecutable() ? path : QString();
#endif
}

QString RcloneUpdater::managedSettingValue() {
  // GetRclone() resolves relative paths against the app folder (Windows) or
  // the AppImage's folder (Linux) in portable mode, so the folder keeps
  // working after being moved or renamed.
  if (IsPortableMode()) {
#ifdef Q_OS_WIN
    return "rclone/" + exeName();
#else
    if (!qEnvironmentVariable("APPIMAGE").isEmpty()) {
      return "rclone/" + exeName();
    }
#endif
  }
  return managedPath();
}

void RcloneUpdater::cleanup() {
  QFile::remove(managedPath() + ".old");
}

int RcloneUpdater::compareVersions(const QString &a, const QString &b) {
  static const QRegularExpression rx(R"((\d+)(?:\.(\d+))?(?:\.(\d+))?)");
  auto parts = [](const QString &v) {
    QList<int> out{0, 0, 0};
    auto m = rx.match(v);
    if (m.hasMatch()) {
      for (int i = 0; i < 3; ++i) {
        out[i] = m.captured(i + 1).toInt();
      }
    }
    return out;
  };
  const auto pa = parts(a), pb = parts(b);
  for (int i = 0; i < 3; ++i) {
    if (pa[i] != pb[i]) {
      return pa[i] < pb[i] ? -1 : 1;
    }
  }
  return 0;
}

QString RcloneUpdater::platformSuffix() {
#if defined(Q_OS_WIN)
  const QString os = "windows";
#elif defined(Q_OS_MACOS)
  const QString os = "osx";
#else
  const QString os = "linux";
#endif
  const QString cpu = QSysInfo::currentCpuArchitecture();
  QString arch = "amd64";
  if (cpu == "arm64") {
    arch = "arm64";
  } else if (cpu == "i386") {
    arch = "386";
  }
  return os + "-" + arch;
}

QNetworkReply *RcloneUpdater::get(const QUrl &url) {
  QNetworkRequest req(url);
  req.setHeader(QNetworkRequest::UserAgentHeader, kUserAgent);
  req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                   QNetworkRequest::NoLessSafeRedirectPolicy);
  req.setTransferTimeout(30000);
  mReply = mNet->get(req);
  return mReply;
}

void RcloneUpdater::fail(const QString &error) {
  mBusy = false;
  mWorkDir.reset();
  emit failed(mCancelled ? tr("Cancelled.") : error);
}

void RcloneUpdater::cancel() {
  mCancelled = true;
  if (mReply) {
    mReply->abort();
  }
  if (mSelfUpdateProcess) {
    mSelfUpdateProcess->kill();
  }
}

// ---------------------------------------------------------------- check ----

void RcloneUpdater::checkLatest() {
  if (mBusy) {
    return;
  }
  mBusy = true;
  mCancelled = false;
  checkFrom(0);
}

void RcloneUpdater::checkFrom(int i) {
  if (i >= mSources.size()) {
    fail(tr("Could not reach rclone.org or GitHub to check the latest "
            "rclone version."));
    return;
  }
  const Source &src = mSources[i];
  const bool useRedirect = src.versionUrl.isEmpty();
  QNetworkReply *reply =
      get(QUrl(useRedirect ? "https://github.com/rclone/rclone/releases/latest"
                           : src.versionUrl));

  connect(reply, &QNetworkReply::finished, this, [=, this]() {
    reply->deleteLater();
    if (mCancelled) {
      fail(QString());
      return;
    }
    static const QRegularExpression rx(R"(v\d+\.\d+\.\d+)");
    QString version;
    if (reply->error() == QNetworkReply::NoError) {
      // version.txt contains "rclone v1.75.1"; the GitHub page redirects to
      // .../releases/tag/v1.75.1
      const QString text = useRedirect ? reply->url().toString()
                                       : QString::fromUtf8(reply->readAll());
      version = rx.match(text).captured(0);
    }
    if (version.isEmpty()) {
      checkFrom(i + 1);
      return;
    }
    mBusy = false;
    emit latestVersionFound(version);
  });
}

// -------------------------------------------------------------- install ----

void RcloneUpdater::install(const QString &version) {
  if (mBusy) {
    return;
  }

  // Fail closed: only a stable version may be selected.
  static const QRegularExpression kStable(R"(^v[0-9]+\.[0-9]+\.[0-9]+$)");
  if (!kStable.match(version).hasMatch()) {
    emit failed(tr("Invalid stable rclone version: %1").arg(version));
    return;
  }

  // Prefer already installed managed rclone. Linux AppImage can fall back
  // to its bundled binary for the first signature-checked update.
  QString updaterExe = managedPath();
  if (!QFileInfo(updaterExe).isExecutable()) {
    updaterExe = GetRclone();
  }
  if (!QFileInfo(updaterExe).isExecutable()) {
    updaterExe = bundledPath();
  }
  if (!QFileInfo(updaterExe).isExecutable()) {
    emit failed(tr("A working rclone executable is required to verify signed "
                   "updates. No change was made."));
    return;
  }

  mBusy = true;
  mCancelled = false;
  mWorkDir = std::make_unique<QTemporaryDir>();
  if (!mWorkDir->isValid()) {
    fail(tr("Cannot create a temporary folder."));
    return;
  }

  const QString stageDir = mWorkDir->filePath("signed-update");
  if (!QDir().mkpath(stageDir)) {
    fail(tr("Cannot create a temporary update folder."));
    return;
  }
  const QString output = QDir(stageDir).filePath(exeName());

  // rclone selfupdate checks the SHA-256 sum and PGP signature using the
  // signing key compiled into the existing rclone executable. --output
  // stages the result without changing the currently installed binary.
  emit status(tr("Downloading rclone %1 and checking its PGP signature…")
                  .arg(version));
  emit progress(0, 0);
  auto *p = new QProcess(this);
  mSelfUpdateProcess = p;

  connect(p, &QProcess::errorOccurred, this,
          [this, p](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart ||
                mSelfUpdateProcess != p) {
              return;
            }
            mSelfUpdateProcess = nullptr;
            p->deleteLater();
            fail(tr("Could not start rclone's signed updater."));
          });
  connect(p, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
          [this, p, version, stageDir, output](int exitCode,
                                                QProcess::ExitStatus exitStatus) {
            if (mSelfUpdateProcess != p) {
              return;
            }
            mSelfUpdateProcess = nullptr;
            const QString details =
                QString::fromUtf8(p->readAllStandardError()).trimmed().left(400);
            p->deleteLater();
            if (mCancelled) {
              fail(QString());
              return;
            }
            if (exitStatus != QProcess::NormalExit || exitCode != 0) {
              fail(tr("Signed rclone update was rejected (exit %1): %2. "
                      "The installed copy was not changed.")
                       .arg(exitCode)
                       .arg(details));
              return;
            }
            if (!QFileInfo(output).isFile()) {
              fail(tr("Signed updater did not produce the expected executable."));
              return;
            }
            finishInstall(version, stageDir);
          });

  // No shell is used: arguments are passed directly to the rclone process.
  p->start(updaterExe, {"selfupdate", "--stable", "--version", version,
                        "--output", output});
}

void RcloneUpdater::finishInstall(const QString &version,
                                  const QString &extractDir) {
  QString newExe;
  QDirIterator it(extractDir, {exeName()}, QDir::Files,
                  QDirIterator::Subdirectories);
  if (it.hasNext()) {
    newExe = it.next();
  }
  if (newExe.isEmpty()) {
    fail(tr("%1 not found in the downloaded archive.").arg(exeName()));
    return;
  }

#ifndef Q_OS_WIN
  // some extractors (python's zipfile) drop the executable bit
  QFile::setPermissions(newExe, QFile::permissions(newExe) | QFileDevice::ExeOwner |
                                    QFileDevice::ExeGroup | QFileDevice::ExeOther);
#endif

  // Sanity check: the new binary must run and report the expected version.
  {
    QProcess p;
    p.start(newExe, {"version"});
    p.waitForFinished(20000);
    const QString out = QString::fromUtf8(p.readAllStandardOutput());
    if (!out.startsWith("rclone " + version)) {
      fail(tr("The downloaded rclone did not run correctly; nothing was "
              "changed."));
      return;
    }
  }

  emit status(tr("Installing…"));
  const QString target = managedPath();
  const QString old = target + ".old";
  QDir().mkpath(QFileInfo(target).absolutePath());
  QFile::remove(old);

  // A running rclone.exe (job, mount) cannot be overwritten, but it can be
  // renamed; running processes continue with the old file.
  if (QFile::exists(target) && !QFile::rename(target, old)) {
    fail(tr("Cannot replace %1. Is the folder read-only?")
             .arg(QDir::toNativeSeparators(target)));
    return;
  }
  if (!QFile::copy(newExe, target)) {
    QFile::rename(old, target);
    fail(tr("Cannot write %1.").arg(QDir::toNativeSeparators(target)));
    return;
  }
  QFile::remove(old); // fails harmlessly if still in use; cleanup() retries

  mBusy = false;
  mWorkDir.reset();
  emit installed(version, target);
}
