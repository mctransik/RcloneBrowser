#include "proxy_process_environment.h"
#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>

class TestProxyProcessEnvironment : public QObject {
  Q_OBJECT
private slots:
  void appProxyOffDoesNotInjectCredentials() {
    QProcessEnvironment base;
    base.insert("PATH", "/bin");
    ProxyProcessEnvironment::Configuration p;
    p.http = "http://alice:secret@proxy.test";
    const auto env = ProxyProcessEnvironment::forRclone(base, p, {});
    QCOMPARE(env.value("PATH"), QString("/bin"));
    QVERIFY(!env.contains("HTTP_PROXY"));
    QVERIFY(!env.contains("RCLONE_CONFIG_PASS"));
    QVERIFY(!base.contains("HTTP_PROXY"));
  }

  void appProxyOnUsesOnlyChildAndBothCaseVariants() {
    QProcessEnvironment base;
    base.insert("OTHER", "unchanged");
    ProxyProcessEnvironment::Configuration p;
    p.enabled = true;
    p.http = "http://username:synthetic@proxy.test:8080";
    p.https = "http://username:second@proxy.test:8443";
    p.noProxy = "localhost,127.0.0.1";
    const auto env = ProxyProcessEnvironment::forRclone(base, p, "invented-password");
    QCOMPARE(env.value("HTTP_PROXY"), p.http);
    QCOMPARE(env.value("http_proxy"), p.http);
    QCOMPARE(env.value("HTTPS_PROXY"), p.https);
    QCOMPARE(env.value("https_proxy"), p.https);
    QCOMPARE(env.value("NO_PROXY"), p.noProxy);
    QCOMPARE(env.value("no_proxy"), p.noProxy);
    QCOMPARE(env.value("RCLONE_CONFIG_PASS"), QString("invented-password"));
    QCOMPARE(env.value("OTHER"), QString("unchanged"));
    QVERIFY(!base.contains("HTTP_PROXY"));
    QVERIFY(!base.contains("RCLONE_CONFIG_PASS"));
  }

  void inheritedProxyIsOverriddenInChildOnly() {
    QProcessEnvironment base;
    base.insert("HTTPS_PROXY", "http://system.test");
    ProxyProcessEnvironment::Configuration p;
    p.enabled = true;
    p.https = "http://app.test";
    const auto env = ProxyProcessEnvironment::forRclone(base, p, {});
    QCOMPARE(env.value("HTTPS_PROXY"), p.https);
    QCOMPARE(base.value("HTTPS_PROXY"), QString("http://system.test"));
  }

  void disablingCustomProxyClearsNewChildValues() {
    QProcessEnvironment base;
    ProxyProcessEnvironment::Configuration p;
    p.enabled = true;
    p.https = "http://initial.test";
    const auto first = ProxyProcessEnvironment::forRclone(base, p, {});
    QVERIFY(first.contains("HTTPS_PROXY"));
    p = {};
    const auto next = ProxyProcessEnvironment::forRclone(base, p, {});
    QVERIFY(!next.contains("HTTPS_PROXY"));
    QVERIFY(!next.contains("https_proxy"));
  }

  void rcloneConfigPasswordWorksWithoutAppProxy() {
    QProcessEnvironment base;
    const auto env = ProxyProcessEnvironment::forRclone(
        base, {}, "mock-password");
    QCOMPARE(env.value("RCLONE_CONFIG_PASS"), QString("mock-password"));
    QVERIFY(!base.contains("RCLONE_CONFIG_PASS"));
    QVERIFY(!env.contains("HTTP_PROXY"));
  }

  void spawnedRcloneProcessReceivesOnlyItsScopedEnvironment() {
    // Real QProcess integration smoke test, with invented values only.
    const QProcessEnvironment parent = QProcessEnvironment::systemEnvironment();
    ProxyProcessEnvironment::Configuration p;
    p.enabled = true;
    p.http = QStringLiteral("http://fakeuser:fakepass@proxy.invalid:10080");
    p.https = QStringLiteral("http://fakeuser:fakepass2@proxy.invalid:10443");
    const QString executable =
        QDir(QCoreApplication::applicationDirPath()).filePath(
#ifdef Q_OS_WIN
            QStringLiteral("proxy_env_probe.exe")
#else
            QStringLiteral("proxy_env_probe")
#endif
        );

    QProcess rclone;
    rclone.setProcessEnvironment(ProxyProcessEnvironment::forRclone(
        parent, p, QStringLiteral("fake-rclone-config-password")));
    rclone.start(executable, {QStringLiteral("--scoped")});
    QVERIFY2(rclone.waitForStarted(10000), "Synthetic rclone probe did not start");
    QVERIFY2(rclone.waitForFinished(10000), "Synthetic rclone probe did not finish");
    QCOMPARE(rclone.exitStatus(), QProcess::NormalExit);
    QCOMPARE(rclone.exitCode(), 0);

    // Emulate an unrelated external program launched by the GUI.
    QProcess external;
    external.setProcessEnvironment(parent);
    external.start(executable, {QStringLiteral("--external")});
    QVERIFY2(external.waitForStarted(10000), "External probe did not start");
    QVERIFY2(external.waitForFinished(10000), "External probe did not finish");
    QCOMPARE(external.exitStatus(), QProcess::NormalExit);
    QCOMPARE(external.exitCode(), 0);

    // The running test process (representing the GUI) is unchanged.
    QCOMPARE(QProcessEnvironment::systemEnvironment(), parent);
  }

  void externalSystemProxyStillInheritedByRclone() {
    QProcessEnvironment base;
    base.insert("HTTPS_PROXY", "http://system-managed.test");
    const auto env = ProxyProcessEnvironment::forRclone(base, {}, {});
    QCOMPARE(env.value("HTTPS_PROXY"), QString("http://system-managed.test"));
  }
};
QTEST_GUILESS_MAIN(TestProxyProcessEnvironment)
#include "test_proxy_process_environment.moc"
