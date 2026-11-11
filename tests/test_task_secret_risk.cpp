#include "task_secret_risk.h"
#include <QtTest>

class TestTaskSecretRisk : public QObject {
  Q_OBJECT
private slots:
  void normalTaskDoesNotWarn() {
    QVERIFY(!TaskSecretRisk::mayContainCredentials(
        "--fast-list --transfers 4 --exclude *.tmp", "C:/Documents",
        "cloud:backup/Reports"));
  }
  void inlineSecretFlagWarns() {
    QVERIFY(TaskSecretRisk::mayContainCredentials(
        "--s3-secret-access-key=synthetic-key", "local:", "remote:"));
  }
  void separatedSecretFlagWarns() {
    QVERIFY(TaskSecretRisk::mayContainCredentials(
        "--drive-client-secret synthetic-value", "local:", "remote:"));
  }
  void authenticatedSourceUrlWarns() {
    QVERIFY(TaskSecretRisk::mayContainCredentials(
        "", "https://someone:fake-pass@example.test/private", "remote:"));
  }
  void queryTokenInDestinationWarns() {
    QVERIFY(TaskSecretRisk::mayContainCredentials(
        "", "remote:dir", "https://example.test/?access_token=synthetic"));
  }
  void authorizationHeaderWarns() {
    QVERIFY(TaskSecretRisk::mayContainCredentials(
        "--header Authorization: Bearer invented.jwt.token", "a:", "b:"));
  }
  void apiKeyHeaderWarns() {
    QVERIFY(TaskSecretRisk::mayContainCredentials(
        "--header X-Api-Key: synthetic-value", "remote:", "local:"));
  }
  void harmlessFilenameWithAtDoesNotWarn() {
    QVERIFY(!TaskSecretRisk::mayContainCredentials(
        "", "C:/files/report@work.txt", "remote:report@work.txt"));
  }
  void flagWithoutValueDoesNotTriggerWarn() {
    QVERIFY(!TaskSecretRisk::mayContainCredentials(
        "--password-file-not-a-real-flag", "local:", "remote:"));
  }
};
QTEST_GUILESS_MAIN(TestTaskSecretRisk)
#include "test_task_secret_risk.moc"
