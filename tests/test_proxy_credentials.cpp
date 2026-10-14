#include "proxy_credentials.h"

#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

class TestProxyCredentials : public QObject {
  Q_OBJECT
private slots:
  void sanitizeCredentials() {
    const QString secret = QStringLiteral(
        "http://alice:pa%40ss@proxy.example:8080");
    QVERIFY(ProxyCredentials::hasUserInfo(secret));
    QCOMPARE(ProxyCredentials::withoutUserInfo(secret),
             QStringLiteral("http://proxy.example:8080"));
    const QString encoded = QStringLiteral(
        "https://name:p%3A%40%2F@proxy.example:8443");
    QVERIFY(ProxyCredentials::hasUserInfo(encoded));
    QCOMPARE(ProxyCredentials::withoutUserInfo(encoded),
             QStringLiteral("https://proxy.example:8443"));
    QVERIFY(!ProxyCredentials::hasUserInfo(
        QStringLiteral("https://proxy.example:8443")));
  }

  void legacyReadWithoutMutation() {
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings s(dir.filePath("settings.ini"), QSettings::IniFormat);
    const QString key = QStringLiteral("Settings/http_proxy");
    const QString raw = QStringLiteral("http://user:secret@proxy.example:8080");
    s.setValue(key, raw);
    QString recovered;
    QVERIFY(ProxyCredentials::load(&s, key, &recovered));
    QCOMPARE(recovered, raw);
    QCOMPARE(s.value(key).toString(), raw);
  }

  void saveAndMigrate() {
#ifdef Q_OS_WIN
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QSettings s(dir.filePath("settings.ini"), QSettings::IniFormat);
    const QString key = QStringLiteral("Settings/https_proxy");
    const QString raw = QStringLiteral("http://test-user:temporary-secret@proxy.example:8080");
    s.setValue(key, raw);
    QString err;
    if (!ProxyCredentials::migrateLegacy(&s, key, true, &err)) {
      QSKIP("Windows CI user session has no usable Credential Manager.");
    }
    s.sync();
    const QString publicUrl = s.value(key).toString();
    QVERIFY(!publicUrl.contains("test-user"));
    QVERIFY(!publicUrl.contains("temporary-secret"));
    QVERIFY(!s.value(key + "CredentialTarget").toString().isEmpty());
    QString loaded;
    QVERIFY2(ProxyCredentials::load(&s, key, &loaded, &err), qPrintable(err));
    QCOMPARE(loaded, raw);
    QVERIFY2(ProxyCredentials::save(
        &s, key, QStringLiteral("http://proxy.example:8080"), true, &err),
        qPrintable(err));
    QCOMPARE(s.value(key).toString(), QStringLiteral("http://proxy.example:8080"));
    QVERIFY(s.value(key + "CredentialTarget").toString().isEmpty());
#else
    QSKIP("Credential Manager is Windows-only; URL parsing tested separately.");
#endif
  }
};

QTEST_GUILESS_MAIN(TestProxyCredentials)
#include "test_proxy_credentials.moc"
