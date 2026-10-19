#include "task_protection.h"
#include <QtTest>

class TestTaskProtection : public QObject {
  Q_OBJECT
private slots:
  void recognizesLegacyDataWithoutChangingIt() {
    const QByteArray legacy = QByteArrayLiteral("JobOptions plaintext data");
    QCOMPARE(TaskProtection::inspectFormat(legacy),
             TaskProtection::Format::LegacyPlaintext);
    QCOMPARE(TaskProtection::inspectFormat(QByteArray()),
             TaskProtection::Format::LegacyPlaintext);
  }

  void rejectsUnknownFutureProtectedVersion() {
    QCOMPARE(TaskProtection::inspectFormat(QByteArrayLiteral("RBTSENC9data")),
             TaskProtection::Format::UnsupportedProtected);
    QCOMPARE(TaskProtection::inspectFormat(QByteArrayLiteral("RBTSENC")),
             TaskProtection::Format::UnsupportedProtected);
    QByteArray out("previous");
    QVERIFY(!TaskProtection::decryptForWindowsUser(
        QByteArrayLiteral("RBTSENC9data"), &out));
    QVERIFY(out.isEmpty());
  }

  void rejectsTruncatedProtectedMagicInsteadOfTreatingItAsLegacy() {
    for (const QByteArray header : {
             QByteArrayLiteral("RBTS"),
             QByteArrayLiteral("RBTSE"),
             QByteArrayLiteral("RBTSEN"),
             QByteArrayLiteral("RBTSENC")}) {
      QCOMPARE(TaskProtection::inspectFormat(header),
               TaskProtection::Format::UnsupportedProtected);
      QByteArray out("old-sensitive-content");
      QVERIFY(!TaskProtection::decryptForWindowsUser(header, &out));
      QVERIFY(out.isEmpty());
    }
    QCOMPARE(TaskProtection::inspectFormat(QByteArrayLiteral("RBTA")),
             TaskProtection::Format::LegacyPlaintext);
  }

  void rejectsPlaintextOrTruncatedProtectedInput() {
    QByteArray out("previous");
    QVERIFY(!TaskProtection::decryptForWindowsUser(
        QByteArrayLiteral("ordinary plaintext"), &out));
    QVERIFY(out.isEmpty());
    QVERIFY(!TaskProtection::decryptForWindowsUser(
        QByteArrayLiteral("RBTSENC1"), &out));
  }

#ifdef Q_OS_WIN
  void roundTripCurrentWindowsUser() {
    const QByteArray secret = QByteArrayLiteral(
        "source=remote:test password=synthetic-test-credential");
    QByteArray encrypted;
    QString error;
    QVERIFY2(TaskProtection::encryptForWindowsUser(secret, &encrypted, &error),
             qPrintable(error));
    QCOMPARE(TaskProtection::inspectFormat(encrypted),
             TaskProtection::Format::WindowsDpapiV1);
    QVERIFY(!encrypted.contains(secret));
    QVERIFY(encrypted.startsWith("RBTSENC1"));
    QByteArray decrypted;
    QVERIFY2(TaskProtection::decryptForWindowsUser(encrypted, &decrypted, &error),
             qPrintable(error));
    QCOMPARE(decrypted, secret);
  }

  void preservesBinaryPayloadIncludingEmptyTaskList() {
    for (const QByteArray plain : {QByteArray(), QByteArray("a\0b\0c", 5)}) {
      QByteArray cipher;
      QByteArray recovered;
      QVERIFY(TaskProtection::encryptForWindowsUser(plain, &cipher));
      QVERIFY(TaskProtection::decryptForWindowsUser(cipher, &recovered));
      QCOMPARE(recovered, plain);
    }
  }

  void detectsTamperedCiphertextWithoutReturningPlaintext() {
    QByteArray cipher;
    QVERIFY(TaskProtection::encryptForWindowsUser(
        QByteArrayLiteral("invented-private-value"), &cipher));
    cipher[cipher.size() - 1] =
        static_cast<char>(cipher.at(cipher.size() - 1) ^ 0x5a);
    QByteArray recovered("stale-secret");
    QVERIFY(!TaskProtection::decryptForWindowsUser(cipher, &recovered));
    QVERIFY(recovered.isEmpty());
  }
#else
  void unsupportedPlatformRefusesEncryptionWithoutFallback() {
    QByteArray result("stale-plaintext");
    QString error;
    QVERIFY(!TaskProtection::encryptForWindowsUser(
        QByteArrayLiteral("private-test-data"), &result, &error));
    QVERIFY(result.isEmpty());
    QVERIFY(!error.isEmpty());
  }
#endif
};

QTEST_GUILESS_MAIN(TestTaskProtection)
#include "test_task_protection.moc"
