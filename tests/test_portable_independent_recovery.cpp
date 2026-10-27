// RESEARCH ONLY: independent, read-only RBTSENC2 recovery fixture.
// This decoder does NOT call the existing test_portable_v2_envelope.cpp open(),
// seal(), policy wrappers or any Windows/Qt credential-store API.
// Never link into the application and NEVER give it real passwords/task files.
#include <QtTest>
#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTemporaryDir>
#include "portable_recovery_lock_policy.h"

#include <sodium.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {
constexpr int kHeaderSize = 106;
constexpr int kWrappedSize = 48;
constexpr int kTagSize = crypto_aead_xchacha20poly1305_ietf_ABYTES;
constexpr int kMaxPlain = 1024 * 1024; // research limit only
constexpr int kMaxWire = kHeaderSize + kWrappedSize + kTagSize + kMaxPlain;
constexpr int kFixtureSize = 202;
constexpr char kExpectedSha[] =
    "6a166dd34aeb98928517b293975dd70be4635eb7271f75c6644acd2769159674";
constexpr char kWrapDomain[] = "RcloneBrowser/RBTSENC2/wrap/1";
constexpr char kPayloadDomain[] = "RcloneBrowser/RBTSENC2/payload/1";
constexpr char kSyntheticPassword[] = "synthetic-fixture-password";
constexpr char kSyntheticPlain[] = "Synthetic portable task bytes v2";

unsigned int kdfCalls = 0;

struct OneKey final {
  std::array<unsigned char, 32> bytes{};
  OneKey() = default;
  OneKey(const OneKey &) = delete;
  OneKey &operator=(const OneKey &) = delete;
  OneKey(OneKey &&) = delete;
  OneKey &operator=(OneKey &&) = delete;
  ~OneKey() noexcept { sodium_memzero(bytes.data(), bytes.size()); }
};

quint32 readBe32(const QByteArray &bytes, int offset) {
  quint32 n = 0;
  for (int i = 0; i != 4; ++i)
    n = (n << 8) | static_cast<unsigned char>(bytes.at(offset + i));
  return n;
}
quint16 readBe16(const QByteArray &bytes, int offset) {
  return static_cast<quint16>(
      (static_cast<unsigned char>(bytes.at(offset)) << 8) |
      static_cast<unsigned char>(bytes.at(offset + 1)));
}

// Standalone decoder: accepts only the deliberately narrow known synthetic
// wire format. No auto-detection, no DPAPI, no filesystem or machine identity,
// no permissive KDF profile negotiation, and no unverified plaintext fallback.
bool independentRead(const QByteArray &wire, const QByteArray &password,
                     QByteArray *plaintext) {
  if (!plaintext || plaintext == &wire || plaintext == &password)
    return false;
  plaintext->clear();

  // BEFORE KDF: reject untrusted sizes, format version and cost fields.
  if (sodium_init() < 0 || password.isEmpty() || password.size() > 4096 ||
      wire.size() < kHeaderSize + kWrappedSize + kTagSize ||
      wire.size() > kMaxWire)
    return false;
  if (wire.left(8) != QByteArray("RBTSENC2", 8) ||
      static_cast<unsigned char>(wire.at(8)) != 1 ||
      static_cast<unsigned char>(wire.at(9)) != 1 ||
      static_cast<unsigned char>(wire.at(10)) != 1 ||
      static_cast<unsigned char>(wire.at(11)) != 1 ||
      readBe32(wire, 12) != 2u || readBe32(wire, 16) != 65536u ||
      readBe16(wire, 100) != kWrappedSize)
    return false;
  const quint32 cipherLen = readBe32(wire, 102);
  if (cipherLen < kTagSize ||
      cipherLen > static_cast<quint32>(kMaxPlain + kTagSize) ||
      wire.size() != kHeaderSize + kWrappedSize + qsizetype(cipherLen))
    return false;

  // Byte offsets documented by independent test vector's wire specification.
  const auto *bytes = reinterpret_cast<const unsigned char *>(wire.constData());
  const unsigned char *salt = bytes + 36;
  const unsigned char *wrapNonce = bytes + 52;
  const unsigned char *payloadNonce = bytes + 76;
  const unsigned char *wrapped = bytes + kHeaderSize;
  const unsigned char *ciphertext = wrapped + kWrappedSize;

  OneKey kek, dek;
  ++kdfCalls;
  if (crypto_pwhash(kek.bytes.data(), kek.bytes.size(),
                    password.constData(),
                    static_cast<unsigned long long>(password.size()), salt,
                    2, static_cast<std::size_t>(65536) * 1024,
                    crypto_pwhash_ALG_ARGON2ID13) != 0)
    return false;

  QByteArray wrapAd(kWrapDomain);
  wrapAd.append(wire.constData(), kHeaderSize);
  unsigned long long opened = 0;
  const int unwrapRc = crypto_aead_xchacha20poly1305_ietf_decrypt(
      dek.bytes.data(), &opened, nullptr,
      wrapped, kWrappedSize,
      reinterpret_cast<const unsigned char *>(wrapAd.constData()),
      static_cast<unsigned long long>(wrapAd.size()),
      wrapNonce, kek.bytes.data());
  sodium_memzero(kek.bytes.data(), kek.bytes.size());
  if (unwrapRc != 0 || opened != dek.bytes.size())
    return false;

  QByteArray payloadAd(kPayloadDomain);
  payloadAd.append(wire.constData(), kHeaderSize);
  payloadAd.append(reinterpret_cast<const char *>(wrapped), kWrappedSize);
  QByteArray staging;
  staging.resize(qsizetype(cipherLen) - kTagSize);
  opened = 0;
  const int payloadRc = crypto_aead_xchacha20poly1305_ietf_decrypt(
      reinterpret_cast<unsigned char *>(staging.data()), &opened, nullptr,
      ciphertext, cipherLen,
      reinterpret_cast<const unsigned char *>(payloadAd.constData()),
      static_cast<unsigned long long>(payloadAd.size()),
      payloadNonce, dek.bytes.data());
  sodium_memzero(dek.bytes.data(), dek.bytes.size());
  if (payloadRc != 0 ||
      opened != static_cast<unsigned long long>(staging.size())) {
    if (!staging.isEmpty())
      sodium_memzero(staging.data(), static_cast<std::size_t>(staging.size()));
    return false;
  }
  *plaintext = std::move(staging);
  return true;
}

QByteArray committedSyntheticFixture() {
  QFile f(QCoreApplication::applicationDirPath() +
          QStringLiteral("/rbtsenc2-synthetic-v1.hex"));
  if (!f.open(QIODevice::ReadOnly))
    return {};
  const QByteArray hex = f.readAll().trimmed();
  if (hex.size() != 2 * kFixtureSize)
    return {};
  const QByteArray raw = QByteArray::fromHex(hex);
  if (raw.size() != kFixtureSize || raw.toHex() != hex ||
      QCryptographicHash::hash(raw, QCryptographicHash::Sha256).toHex() !=
          QByteArray(kExpectedSha))
    return {};
  return raw;
}

bool saveProtectedCopy(const QString &filename, const QByteArray &wire) {
  QSaveFile out(filename);
  out.setDirectWriteFallback(false);
  return out.open(QIODevice::WriteOnly) &&
         out.write(wire) == qint64(wire.size()) &&
         out.commit();
}

// Synthetic-only transactional recovery experiment. NOT production code:
// the "OS lock" result and consent are caller-supplied test values, not OS
// attestation or an implemented GUI consent. The destination may contain
// previously restored test data, which must survive all error paths.
enum class RestoreFault { None, CancelAfterPartialWrite, CancelBeforeCommit };
enum class RestoreState {
  UnsafeTarget,
  DeniedByPolicy,
  ConsentRequired,
  AuthenticationFailed,
  StorageFailed,
  Written
};
struct RestoreOutcome {
  RestoreState state;
  bool degraded;
};

RestoreOutcome researchRestoreAtomically(
    const QByteArray &encryptedBytes, const QByteArray &password,
    const QString &sourceEncryptedPath, const QString &destinationPath,
    rb_research_lock::LockPolicy policy,
    rb_research_lock::LockAttempt lockAttempt,
    bool explicitDegradedConsent,
    RestoreFault simulatedFault = RestoreFault::None) {
  const QFileInfo source(sourceEncryptedPath), destination(destinationPath);
  // This small test-only safeguard prevents overwriting the encrypted
  // source through identical paths or obvious aliases. Full production
  // path/symlink/race-hardening remains UNIMPLEMENTED.
  const QString canonicalSource = source.canonicalFilePath();
  if (!source.exists() || canonicalSource.isEmpty() ||
      destinationPath.isEmpty() ||
      source.absoluteFilePath() == destination.absoluteFilePath() ||
      (!destination.canonicalFilePath().isEmpty() &&
       canonicalSource == destination.canonicalFilePath()))
    return {RestoreState::UnsafeTarget, false};

  const auto decision = rb_research_lock::evaluate(
      policy, rb_research_lock::Purpose::RecoverExisting, lockAttempt);
  if (!decision.allowed)
    return {RestoreState::DeniedByPolicy, false};
  if (decision.degraded && !explicitDegradedConsent)
    return {RestoreState::ConsentRequired, true};

  QByteArray recovered;
  if (!independentRead(encryptedBytes, password, &recovered))
    return {RestoreState::AuthenticationFailed, decision.degraded};

  // With direct-write fallback *disabled*, a failure cannot quietly write
  // straight into the old destination. Atomic replacement is QSaveFile's
  // intended contract, but does not prove power-loss durability on every FS.
  QSaveFile output(destinationPath);
  output.setDirectWriteFallback(false);
  if (!output.open(QIODevice::WriteOnly))
    return {RestoreState::StorageFailed, decision.degraded};

  if (simulatedFault == RestoreFault::CancelAfterPartialWrite) {
    const auto half = recovered.size() / 2;
    if (half > 0)
      output.write(recovered.constData(), half);
    output.cancelWriting();
    return {RestoreState::StorageFailed, decision.degraded};
  }

  if (output.write(recovered) != qint64(recovered.size())) {
    output.cancelWriting();
    return {RestoreState::StorageFailed, decision.degraded};
  }
  if (simulatedFault == RestoreFault::CancelBeforeCommit) {
    output.cancelWriting();
    return {RestoreState::StorageFailed, decision.degraded};
  }
  if (!output.commit())
    return {RestoreState::StorageFailed, decision.degraded};
  return {RestoreState::Written, decision.degraded};
}

} // namespace

class TestPortableIndependentRecovery final : public QObject {
  Q_OBJECT
private slots:
  void initTestCase() { QVERIFY(sodium_init() >= 0); }

  void crossDirectoryFixtureDecodesWithoutSourceAccountOrMachineState() {
    const QByteArray protectedBytes = committedSyntheticFixture();
    QCOMPARE(protectedBytes.size(), kFixtureSize);
    QTemporaryDir originatingHost, restoredHost;
    QVERIFY(originatingHost.isValid());
    QVERIFY(restoredHost.isValid());
    const QString fromPath = originatingHost.filePath(
        QStringLiteral("synthetic-backups/source-encrypted-file.rbt"));
    const QString toPath = restoredHost.filePath(
        QStringLiteral("other-account/restored-encrypted-file.rbt"));
    QVERIFY(QDir().mkpath(QFileInfo(fromPath).absolutePath()));
    QVERIFY(QDir().mkpath(QFileInfo(toPath).absolutePath()));
    QVERIFY(saveProtectedCopy(fromPath, protectedBytes));
    QVERIFY(QFile::copy(fromPath, toPath));
    QFile restoredFile(toPath);
    QVERIFY(restoredFile.open(QIODevice::ReadOnly));
    const QByteArray importedBytes = restoredFile.readAll();
    QCOMPARE(restoredFile.error(), QFileDevice::NoError);
    restoredFile.close();
    QCOMPARE(importedBytes, protectedBytes);
    QCOMPARE(QCryptographicHash::hash(importedBytes, QCryptographicHash::Sha256).toHex(),
             QByteArray(kExpectedSha));

    QByteArray clear("stale artificial plaintext");
    const unsigned int before = kdfCalls;
    QVERIFY(independentRead(importedBytes, QByteArray(kSyntheticPassword),
                            &clear));
    QCOMPARE(kdfCalls, before + 1);
    QCOMPARE(clear, QByteArray(kSyntheticPlain));
    // The decoder only needs identical on-disk format + test password.
    // No machine ID, Windows account, DPAPI store or real cloud required.
    QFile original(fromPath), copied(toPath);
    QVERIFY(original.open(QIODevice::ReadOnly));
    QCOMPARE(original.readAll(), protectedBytes);
    QVERIFY(copied.open(QIODevice::ReadOnly));
    QCOMPARE(copied.readAll(), protectedBytes);
  }

  void wrongPasswordAndAuthenticityFailuresLeaveEncryptedSourceUntouched() {
    QByteArray protectedBytes = committedSyntheticFixture();
    QCOMPARE(protectedBytes.size(), kFixtureSize);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString path = tmp.filePath(QStringLiteral("immutable-fixture.rbt"));
    QVERIFY(saveProtectedCopy(path, protectedBytes));

    const unsigned int start = kdfCalls;
    QByteArray clear("stale");
    QVERIFY(!independentRead(protectedBytes, QByteArray("incorrect-public-test"),
                             &clear));
    QVERIFY(clear.isEmpty());
    QCOMPARE(kdfCalls, start + 1);

    QByteArray altered = protectedBytes;
    altered[altered.size() - 1] =
        char(static_cast<unsigned char>(altered.back()) ^ 1u);
    QVERIFY(!independentRead(altered, QByteArray(kSyntheticPassword), &clear));
    QVERIFY(clear.isEmpty());
    QCOMPARE(kdfCalls, start + 2);

    altered = protectedBytes;
    altered[106] = char(static_cast<unsigned char>(altered.at(106)) ^ 1u);
    QVERIFY(!independentRead(altered, QByteArray(kSyntheticPassword), &clear));
    QVERIFY(clear.isEmpty());
    QCOMPARE(kdfCalls, start + 3);

    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    QCOMPARE(saved.readAll(), protectedBytes);
  }

  void transactionalRestorePreservesPreviousOutputOnInterruptions() {
    // Every file is synthetic and confined to a disposable directory.
    using namespace rb_research_lock;
    const QByteArray cipher = committedSyntheticFixture();
    QCOMPARE(cipher.size(), kFixtureSize);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.filePath(
        QStringLiteral("encrypted-synthetic-backup.rbt"));
    const QString destination = tmp.filePath(
        QStringLiteral("already-restored-synthetic-output.txt"));
    QVERIFY(saveProtectedCopy(source, cipher));
    const QByteArray previous("PREVIOUS SYNTHETIC OUTPUT MUST SURVIVE");
    QVERIFY(saveProtectedCopy(destination, previous));
    const unsigned int baseline = kdfCalls;

    // No locking verification => deny BEFORE any KDF and file I/O.
    auto decision = researchRestoreAtomically(
        cipher, QByteArray(kSyntheticPassword), source, destination,
        LockPolicy::RecoveryOnly, LockAttempt::NotAttempted, true);
    QCOMPARE(decision.state, RestoreState::DeniedByPolicy);
    QCOMPARE(kdfCalls, baseline);

    // Actual locking failure cannot silently override missing consent.
    decision = researchRestoreAtomically(
        cipher, QByteArray(kSyntheticPassword), source, destination,
        LockPolicy::RecoveryOnly, LockAttempt::Failed, false);
    QCOMPARE(decision.state, RestoreState::ConsentRequired);
    QVERIFY(decision.degraded);
    QCOMPARE(kdfCalls, baseline);

    // Even explicit consent cannot override Strict's refusal.
    decision = researchRestoreAtomically(
        cipher, QByteArray(kSyntheticPassword), source, destination,
        LockPolicy::Strict, LockAttempt::Failed, true);
    QCOMPARE(decision.state, RestoreState::DeniedByPolicy);
    QCOMPARE(kdfCalls, baseline);

    // Interrupted staged output (including a partial write) leaves existing
    // output unchanged and never overwrites the encrypted source.
    for (const RestoreFault fault : {RestoreFault::CancelAfterPartialWrite,
                                     RestoreFault::CancelBeforeCommit}) {
      decision = researchRestoreAtomically(
          cipher, QByteArray(kSyntheticPassword), source, destination,
          LockPolicy::RecoveryOnly, LockAttempt::Failed, true, fault);
      QCOMPARE(decision.state, RestoreState::StorageFailed);
      QVERIFY(decision.degraded);
      QFile preserved(destination), encrypted(source);
      QVERIFY(preserved.open(QIODevice::ReadOnly));
      QCOMPARE(preserved.readAll(), previous);
      QVERIFY(encrypted.open(QIODevice::ReadOnly));
      QCOMPARE(encrypted.readAll(), cipher);
    }
    QCOMPARE(kdfCalls, baseline + 2);

    // Successful recovery can replace prior synthetic output atomically.
    decision = researchRestoreAtomically(
        cipher, QByteArray(kSyntheticPassword), source, destination,
        LockPolicy::Strict, LockAttempt::Succeeded, false);
    QCOMPARE(decision.state, RestoreState::Written);
    QVERIFY(!decision.degraded);
    QFile recovered(destination), stillEncrypted(source);
    QVERIFY(recovered.open(QIODevice::ReadOnly));
    QCOMPARE(recovered.readAll(), QByteArray(kSyntheticPlain));
    QVERIFY(stillEncrypted.open(QIODevice::ReadOnly));
    QCOMPARE(stillEncrypted.readAll(), cipher);
    QCOMPARE(kdfCalls, baseline + 3);
  }

  void failedAuthenticationCannotOverwriteRestoredOutputOrEncryptedInput() {
    using namespace rb_research_lock;
    const QByteArray cipher = committedSyntheticFixture();
    QCOMPARE(cipher.size(), kFixtureSize);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.filePath(QStringLiteral("source.rbt"));
    const QString destination = tmp.filePath(QStringLiteral("destination.txt"));
    QVERIFY(saveProtectedCopy(source, cipher));
    const QByteArray previous("PREVIOUS RESTORED CONTENT");
    QVERIFY(saveProtectedCopy(destination, previous));
    const unsigned int before = kdfCalls;

    auto x = researchRestoreAtomically(
        cipher, QByteArray("wrong synthetic password"), source, destination,
        LockPolicy::RecoveryOnly, LockAttempt::Failed, true);
    QCOMPARE(x.state, RestoreState::AuthenticationFailed);
    QVERIFY(x.degraded);

    QByteArray tampered = cipher;
    tampered[tampered.size() - 1] =
        char(static_cast<unsigned char>(tampered.back()) ^ 1u);
    x = researchRestoreAtomically(
        tampered, QByteArray(kSyntheticPassword), source, destination,
        LockPolicy::RecoveryOnly, LockAttempt::Failed, true);
    QCOMPARE(x.state, RestoreState::AuthenticationFailed);
    QCOMPARE(kdfCalls, before + 2);

    // Invalid KDF header must fail BEFORE spending Argon2id resources.
    tampered = cipher;
    tampered[12] = char(0xff);
    x = researchRestoreAtomically(
        tampered, QByteArray(kSyntheticPassword), source, destination,
        LockPolicy::RecoveryOnly, LockAttempt::Failed, true);
    QCOMPARE(x.state, RestoreState::AuthenticationFailed);
    QCOMPARE(kdfCalls, before + 2);

    QFile dest(destination), encrypted(source);
    QVERIFY(dest.open(QIODevice::ReadOnly));
    QCOMPARE(dest.readAll(), previous);
    QVERIFY(encrypted.open(QIODevice::ReadOnly));
    QCOMPARE(encrypted.readAll(), cipher);

    // Never clobber the original encrypted source, even with a correct pw.
    x = researchRestoreAtomically(
        cipher, QByteArray(kSyntheticPassword), source, source,
        LockPolicy::Strict, LockAttempt::Succeeded, false);
    QCOMPARE(x.state, RestoreState::UnsafeTarget);
    QCOMPARE(kdfCalls, before + 2);
  }

  void interruptedNewRestoreLeavesNoPartialFile() {
    using namespace rb_research_lock;
    const QByteArray cipher = committedSyntheticFixture();
    QCOMPARE(cipher.size(), kFixtureSize);
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());
    const QString source = tmp.filePath(QStringLiteral("encrypted-only.rbt"));
    const QString destination = tmp.filePath(QStringLiteral("new-restore.txt"));
    QVERIFY(saveProtectedCopy(source, cipher));
    QVERIFY(!QFile::exists(destination));

    auto x = researchRestoreAtomically(
        cipher, QByteArray(kSyntheticPassword), source, destination,
        LockPolicy::Strict, LockAttempt::Succeeded, false,
        RestoreFault::CancelAfterPartialWrite);
    QCOMPARE(x.state, RestoreState::StorageFailed);
    QVERIFY(!QFile::exists(destination));

    x = researchRestoreAtomically(
        cipher, QByteArray(kSyntheticPassword), source, destination,
        LockPolicy::Strict, LockAttempt::Succeeded, false);
    QCOMPARE(x.state, RestoreState::Written);
    QFile out(destination);
    QVERIFY(out.open(QIODevice::ReadOnly));
    QCOMPARE(out.readAll(), QByteArray(kSyntheticPlain));
  }

  void hostileMetadataRefusedBeforeAnyArgon2idWork() {
    const QByteArray fixture = committedSyntheticFixture();
    QCOMPARE(fixture.size(), kFixtureSize);
    const unsigned int prior = kdfCalls;
    QByteArray output("outdated synthetic output");
    auto reject = [&](QByteArray hostile) {
      output = "stale";
      QVERIFY(!independentRead(hostile, QByteArray(kSyntheticPassword),
                               &output));
      QVERIFY(output.isEmpty());
      QCOMPARE(kdfCalls, prior);
    };

    QByteArray bad = fixture;
    bad.replace(0, 8, QByteArray("RBTSENC1", 8));
    reject(bad);
    bad = fixture;
    bad[11] = char(2); // another slot is deliberately unsupported
    reject(bad);
    bad = fixture;
    bad[12] = char(0xff); // hostile Argon2id cost
    reject(bad);
    bad = fixture;
    bad[16] = char(0xff); // hostile memory limit
    reject(bad);
    bad = fixture;
    bad[102] = char(0x7f); // impossible ciphertext length
    reject(bad);
    reject(fixture.left(kHeaderSize));
    reject(fixture.left(kFixtureSize - 1));
    reject(fixture + QByteArray("TRAILING", 8));

    QByteArray mutableWire = fixture;
    const QByteArray originalCopy = mutableWire;
    QVERIFY(!independentRead(mutableWire, QByteArray(kSyntheticPassword),
                             &mutableWire));
    QCOMPARE(mutableWire, originalCopy);
  }
};
QTEST_GUILESS_MAIN(TestPortableIndependentRecovery)
#include "test_portable_independent_recovery.moc"
