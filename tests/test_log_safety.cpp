#include "log_safety.h"
#include <QtTest>

class TestLogSafety : public QObject {
  Q_OBJECT
private slots:
  void masksUrlUserinfo() {
    const QString s = LogSafety::maskKnownCredentials(
        QStringLiteral("https://alice:p%40ss@proxy.test:8443/dir "
                       "http://bob:other@host.test/status"));
    QVERIFY(!s.contains(QStringLiteral("p%40ss")));
    QVERIFY(!s.contains(QStringLiteral("bob:other")));
    QVERIFY(s.contains(QStringLiteral("https://[REDACTED]@proxy.test")));
  }
  void masksQueryTokensButKeepsOrdinaryFields() {
    const QString s = LogSafety::maskKnownCredentials(
        QStringLiteral("https://host.test/?access_token=verysecret&v=2&client_secret=hidden"));
    QVERIFY(!s.contains(QStringLiteral("verysecret")));
    QVERIFY(!s.contains(QStringLiteral("hidden")));
    QVERIFY(s.contains(QStringLiteral("&v=2")));
  }
  void masksAuthHeadersAndEnvironment() {
    const QString s = LogSafety::maskKnownCredentials(
        QStringLiteral("Authorization: Bearer abc.123\n"
                       "Proxy-Authorization: Basic YWJjMTIz\n"
                       "RCLONE_CONFIG_PASS=somepass AWS_SECRET_ACCESS_KEY=topsecret"));
    QVERIFY(!s.contains(QStringLiteral("abc.123")));
    QVERIFY(!s.contains(QStringLiteral("YWJjMTIz")));
    QVERIFY(!s.contains(QStringLiteral("somepass")));
    QVERIFY(!s.contains(QStringLiteral("topsecret")));
  }
  void masksSecretFlags() {
    const QString s = LogSafety::maskKnownCredentials(
        QStringLiteral("--password=mypassword --token mytoken --client-secret=\"two words\""));
    QVERIFY(!s.contains(QStringLiteral("mypassword")));
    QVERIFY(!s.contains(QStringLiteral("mytoken")));
    QVERIFY(!s.contains(QStringLiteral("two words")));
  }
  void preservesRegularRcloneStatisticsAndPaths() {
    const QString stats = QStringLiteral(
        "Transferred: 10 MiB / 20 MiB, 50%, 1 MiB/s, ETA 10s");
    QCOMPARE(LogSafety::maskKnownCredentials(stats), stats);
    const QString path = QStringLiteral(
        "remote:folder/file@home.txt: Copied (new)");
    QCOMPARE(LogSafety::maskKnownCredentials(path), path);
  }
  void handlesUppercaseScheme() {
    const QString s = LogSafety::maskKnownCredentials(
        QStringLiteral("HTTPS://user:secret@proxy.test/path"));
    QVERIFY(!s.contains(QStringLiteral("user:secret")));
    QVERIFY(s.contains(QStringLiteral("[REDACTED]")));
  }
};
QTEST_GUILESS_MAIN(TestLogSafety)
#include "test_log_safety.moc"
