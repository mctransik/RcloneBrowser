#include "credential_safety.h"

#include <QtTest>

class TestCredentialSafety : public QObject {
  Q_OBJECT

private slots:
  void detectsHintAtBeginning() {
    QVERIFY(CredentialSafety::requiresConfigPassword(
        "RCLONE_CONFIG_PASS is required to decrypt config"));
  }

  void detectsHintInsideMessage() {
    QVERIFY(CredentialSafety::requiresConfigPassword(
        "2026/10/13 ERROR: config is encrypted; set RCLONE_CONFIG_PASS"));
  }

  void doesNotTriggerOnUnrelatedErrors() {
    QVERIFY(!CredentialSafety::requiresConfigPassword(
        "network is offline"));
    QVERIFY(!CredentialSafety::requiresConfigPassword(QString()));
  }
};

QTEST_GUILESS_MAIN(TestCredentialSafety)
#include "test_credential_safety.moc"
