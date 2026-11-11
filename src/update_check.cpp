#include "update_check.h"

#include <QList>
#include <QRegularExpression>

namespace UpdateCheck {

int compareVersions(const QString &a, const QString &b) {
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

QString findVersionTag(const QString &text) {
  static const QRegularExpression rx(R"(v\d+\.\d+\.\d+)");
  return rx.match(text).captured(0);
}

bool isStableVersionTag(const QString &version) {
  static const QRegularExpression rx(R"(^v[0-9]+\.[0-9]+\.[0-9]+$)");
  return rx.match(version).hasMatch();
}

QStringList selfupdateArguments(const QString &version,
                                const QString &outputPath) {
  return {"selfupdate", "--stable", "--version", version, "--output",
          outputPath};
}

QUrl updaterProxy(bool useProxy, const QString &httpsProxy,
                  const QString &httpProxy, bool *invalid) {
  if (invalid) {
    *invalid = false;
  }
  if (!useProxy) {
    return QUrl();
  }
  QString text = httpsProxy.trimmed();
  if (text.isEmpty()) {
    text = httpProxy.trimmed();
  }
  const QUrl proxy(text);
  if (!proxy.isValid() || proxy.host().isEmpty() ||
      (proxy.scheme() != QLatin1String("http") &&
       proxy.scheme() != QLatin1String("https"))) {
    if (invalid) {
      *invalid = true;
    }
    return QUrl();
  }
  return proxy;
}

bool reportsUpToDate(const QString &output) {
  return output.contains("rclone is up to date", Qt::CaseInsensitive);
}

} // namespace UpdateCheck
