#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QString>

int main(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  const QProcessEnvironment e = QProcessEnvironment::systemEnvironment();
  const QString http =
      QStringLiteral("http://fakeuser:fakepass@proxy.invalid:10080");
  const QString https =
      QStringLiteral("http://fakeuser:fakepass2@proxy.invalid:10443");
  const QString password = QStringLiteral("fake-rclone-config-password");

  if (app.arguments().contains(QStringLiteral("--scoped"))) {
    return e.value(QStringLiteral("HTTP_PROXY")) == http &&
                   e.value(QStringLiteral("http_proxy")) == http &&
                   e.value(QStringLiteral("HTTPS_PROXY")) == https &&
                   e.value(QStringLiteral("https_proxy")) == https &&
                   e.value(QStringLiteral("RCLONE_CONFIG_PASS")) == password
               ? 0 : 2;
  }
  if (app.arguments().contains(QStringLiteral("--external"))) {
    return e.value(QStringLiteral("HTTP_PROXY")) != http &&
                   e.value(QStringLiteral("HTTPS_PROXY")) != https &&
                   e.value(QStringLiteral("RCLONE_CONFIG_PASS")) != password
               ? 0 : 3;
  }
  return 4;
}
