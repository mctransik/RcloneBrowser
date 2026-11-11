#include "rclone_args.h"

#include <QtTest>

using namespace RcloneArgs;

class TestRcloneArgs : public QObject {
  Q_OBJECT

private slots:
  void empty() {
    QVERIFY(splitOptions("").isEmpty());
    QVERIFY(splitOptions("   ").isEmpty());
  }
  void simple() {
    QCOMPARE(splitOptions("--fast-list --checkers 8"),
             QStringList({"--fast-list", "--checkers", "8"}));
  }
  void repeatedAndOuterWhitespace() {
    // split(' ') used to produce empty arguments here, which rclone treats
    // as an extra positional argument (source/destination).
    QCOMPARE(splitOptions("  --fast-list   --checkers  8 "),
             QStringList({"--fast-list", "--checkers", "8"}));
  }
  void tabs() {
    QCOMPARE(splitOptions("--fast-list\t--checkers\t8"),
             QStringList({"--fast-list", "--checkers", "8"}));
  }
  void quotedValueWithSpaces() {
    QCOMPARE(splitOptions("--exclude \"My Files/**\" --fast-list"),
             QStringList({"--exclude", "My Files/**", "--fast-list"}));
  }
  void quotedInsideArgument() {
    QCOMPARE(splitOptions("--exclude=\"My Files/**\""),
             QStringList({"--exclude=My Files/**"}));
  }
  void windowsPathBackslashes() {
    QCOMPARE(splitOptions("--log-file \"C:\\Users\\Me\\rclone log.txt\""),
             QStringList({"--log-file", "C:\\Users\\Me\\rclone log.txt"}));
    QCOMPARE(splitOptions("--log-file C:\\temp\\rclone.log"),
             QStringList({"--log-file", "C:\\temp\\rclone.log"}));
  }
  void emptyQuotedDropped() {
    QCOMPARE(splitOptions("--fast-list \"\" -v"),
             QStringList({"--fast-list", "-v"}));
  }
  void mountStyleOptions() {
    QCOMPARE(splitOptions("--vfs-cache-mode writes --dir-cache-time 10m"),
             QStringList({"--vfs-cache-mode", "writes", "--dir-cache-time", "10m"}));
  }
};

QTEST_GUILESS_MAIN(TestRcloneArgs)
#include "test_rclone_args.moc"
