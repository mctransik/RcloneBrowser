#include "update_check.h"

#include <QtTest>

using namespace UpdateCheck;

class TestUpdateCheck : public QObject {
  Q_OBJECT

private slots:
  // ---- compareVersions ----
  void compareEqual() {
    QCOMPARE(compareVersions("v1.75.1", "1.75.1"), 0);
    QCOMPARE(compareVersions("rclone v1.75.1", "v1.75.1"), 0);
  }
  void compareNumericNotLexical() {
    // 1.100 > 1.99 (a string comparison would get this wrong)
    QCOMPARE(compareVersions("v1.100.0", "v1.99.0"), 1);
    QCOMPARE(compareVersions("v1.9.0", "v1.10.0"), -1);
  }
  void comparePatchAndMajor() {
    QCOMPARE(compareVersions("v1.75.2", "v1.75.1"), 1);
    QCOMPARE(compareVersions("v1.75.1", "v1.75.2"), -1);
    QCOMPARE(compareVersions("v2.0.0", "v1.99.99"), 1);
  }
  void compareMissingParts() {
    QCOMPARE(compareVersions("1.75", "1.75.0"), 0);
    QCOMPARE(compareVersions("2", "1.99.99"), 1);
  }
  void compareSuffixIgnored() {
    QCOMPARE(compareVersions("1.76.0-beta.8123.abcdef", "v1.76.0"), 0);
  }
  void compareGarbage() {
    QCOMPARE(compareVersions("", ""), 0);
    QCOMPARE(compareVersions("unknown", "v1.0.0"), -1);
  }

  // ---- findVersionTag ----
  void versionFromVersionTxt() {
    QCOMPARE(findVersionTag("rclone v1.75.1\n"), QString("v1.75.1"));
  }
  void versionFromGithubRedirect() {
    QCOMPARE(findVersionTag(
                 "https://github.com/rclone/rclone/releases/tag/v1.75.1"),
             QString("v1.75.1"));
  }
  void versionMissing() {
    QVERIFY(findVersionTag("").isEmpty());
    QVERIFY(findVersionTag("<html>Service Unavailable</html>").isEmpty());
    QVERIFY(findVersionTag("v1.75").isEmpty());
  }

  // ---- isStableVersionTag: install() must fail closed ----
  void stableAccepted() {
    QVERIFY(isStableVersionTag("v1.75.1"));
    QVERIFY(isStableVersionTag("v10.200.3000"));
  }
  void stableRejected() {
    QVERIFY(!isStableVersionTag(""));
    QVERIFY(!isStableVersionTag("1.75.1"));            // no "v"
    QVERIFY(!isStableVersionTag("v1.75"));             // incomplete
    QVERIFY(!isStableVersionTag("v1.76.0-beta.8123")); // beta
    QVERIFY(!isStableVersionTag(" v1.75.1"));
    QVERIFY(!isStableVersionTag("v1.75.1\n"));
    QVERIFY(!isStableVersionTag("rclone v1.75.1"));
  }
  void stableRejectsArgumentInjection() {
    // The tag becomes a command-line argument of rclone selfupdate.
    QVERIFY(!isStableVersionTag("v1.75.1 --output x"));
    QVERIFY(!isStableVersionTag("--beta"));
    QVERIFY(!isStableVersionTag("v1.75.1/../.."));
  }
  void foundTagIsStable() {
    // whatever checkLatest() extracts must pass install()'s check
    QVERIFY(isStableVersionTag(findVersionTag("rclone v1.75.1\n")));
  }

  // ---- selfupdateArguments ----
  void selfupdateCommandLine() {
    QCOMPARE(selfupdateArguments("v1.75.1", "/tmp/x/rclone"),
             QStringList({"selfupdate", "--stable", "--version", "v1.75.1",
                          "--output", "/tmp/x/rclone"}));
  }
  void selfupdateOutputWithSpaces() {
    // paths with spaces stay one argument (no shell is involved)
    const QStringList args =
        selfupdateArguments("v1.75.1", "C:\\Users\\A B\\Temp\\rclone.exe");
    QCOMPARE(args.size(), 6);
    QCOMPARE(args.last(), QString("C:\\Users\\A B\\Temp\\rclone.exe"));
  }
  void selfupdateNeverBeta() {
    QVERIFY(!selfupdateArguments("v1.75.1", "o").contains("--beta"));
    QVERIFY(selfupdateArguments("v1.75.1", "o").contains("--stable"));
  }

  // ---- updaterProxy ----
  void proxyDisabled() {
    bool invalid = true;
    QVERIFY(!updaterProxy(false, "http://proxy:3128", QString(), &invalid)
                 .isValid());
    QVERIFY(!invalid);
  }
  void proxyHttpsPreferred() {
    bool invalid = true;
    const QUrl p = updaterProxy(true, "http://secure:3128", "http://plain:8080",
                                &invalid);
    QVERIFY(!invalid);
    QCOMPARE(p.host(), QString("secure"));
    QCOMPARE(p.port(), 3128);
  }
  void proxyFallsBackToHttp() {
    bool invalid = true;
    const QUrl p = updaterProxy(true, "  ", " http://plain:8080 ", &invalid);
    QVERIFY(!invalid);
    QCOMPARE(p.host(), QString("plain"));
  }
  void proxyHttpsScheme() {
    bool invalid = true;
    QVERIFY(updaterProxy(true, "https://p.example:443", QString(), &invalid)
                .isValid());
    QVERIFY(!invalid);
  }
  void proxyWithCredentialsKept() {
    bool invalid = true;
    const QUrl p =
        updaterProxy(true, "http://user:secret@p.example:3128", QString(), &invalid);
    QVERIFY(!invalid);
    QCOMPARE(p.userName(), QString("user"));
    QCOMPARE(p.password(), QString("secret"));
  }
  void proxyEnabledButEmptyIsInvalid() {
    bool invalid = false;
    QVERIFY(!updaterProxy(true, QString(), QString(), &invalid).isValid());
    QVERIFY(invalid);
  }
  void proxyRejectsOtherSchemes() {
    for (const QString &bad :
         {QString("socks5://p:1080"), QString("ftp://p:21"),
          QString("file:///etc/passwd"), QString("p.example:3128"),
          QString("http://"), QString("not a url")}) {
      bool invalid = false;
      QVERIFY2(!updaterProxy(true, bad, QString(), &invalid).isValid(),
               qPrintable(bad));
      QVERIFY2(invalid, qPrintable(bad));
    }
  }
  void proxyNullInvalidPointer() {
    QVERIFY(!updaterProxy(true, "socks5://p:1080", QString(), nullptr).isValid());
  }

  // ---- reportsUpToDate ----
  void upToDateDetected() {
    QVERIFY(reportsUpToDate("2026/11/11 INFO  : rclone is up to date"));
    QVERIFY(reportsUpToDate("Rclone is UP TO DATE"));
  }
  void upToDateNotDetected() {
    QVERIFY(!reportsUpToDate(""));
    QVERIFY(!reportsUpToDate("Successfully updated rclone to version v1.75.1"));
    QVERIFY(!reportsUpToDate("signature verification failed"));
  }
};

QTEST_GUILESS_MAIN(TestUpdateCheck)
#include "test_update_check.moc"
