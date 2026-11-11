#pragma once
#include <QProcessEnvironment>
#include <QString>

// The application's proxy credentials belong in rclone child environments,
// not in the GUI's global environment or unrelated external programs.
namespace ProxyProcessEnvironment {
struct Configuration {
  bool enabled = false;
  QString http;
  QString https;
  QString noProxy;
};
inline QProcessEnvironment forRclone(const QProcessEnvironment &inherited,
                                     const Configuration &proxy,
                                     const QString &configPassword) {
  QProcessEnvironment env = inherited;
  if (proxy.enabled) {
    env.insert(QStringLiteral("HTTP_PROXY"), proxy.http);
    env.insert(QStringLiteral("http_proxy"), proxy.http);
    env.insert(QStringLiteral("HTTPS_PROXY"), proxy.https);
    env.insert(QStringLiteral("https_proxy"), proxy.https);
    env.insert(QStringLiteral("NO_PROXY"), proxy.noProxy);
    env.insert(QStringLiteral("no_proxy"), proxy.noProxy);
  }
  if (!configPassword.isEmpty())
    env.insert(QStringLiteral("RCLONE_CONFIG_PASS"), configPassword);
  return env;
}
} // namespace ProxyProcessEnvironment
