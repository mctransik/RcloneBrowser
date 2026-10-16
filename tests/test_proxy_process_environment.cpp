#include "proxy_process_environment.h"
#include <QtTest>

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

  void externalSystemProxyStillInheritedByRclone() {
    QProcessEnvironment base;
    base.insert("HTTPS_PROXY", "http://system-managed.test");
    const auto env = ProxyProcessEnvironment::forRclone(base, {}, {});
    QCOMPARE(env.value("HTTPS_PROXY"), QString("http://system-managed.test"));
  }
};
QTEST_GUILESS_MAIN(TestProxyProcessEnvironment)
#include "test_proxy_process_environment.moc"
