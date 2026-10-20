// DEVELOPMENT PROBE ONLY. Not included in the RcloneBrowser executable.
// No persistent RBTSENC2 file format, task migration or user credential use.
#include <QtTest>
#include <QByteArray>

#include <array>
#include <sodium.h>

namespace {
constexpr char kWrapDomain[] = "RcloneBrowser/probe/dek-wrap/v0";
constexpr char kDataDomain[] = "RcloneBrowser/probe/payload/v0";

// Fixed test-only Argon2id settings, NOT production policy. Production work
// factors must be benchmarked and authenticated in a reviewed binary format.
constexpr unsigned long long kOps = crypto_pwhash_OPSLIMIT_INTERACTIVE;
constexpr size_t kMem = crypto_pwhash_MEMLIMIT_INTERACTIVE;

using Key = std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_KEYBYTES>;
using Salt = std::array<unsigned char, crypto_pwhash_SALTBYTES>;
using Nonce = std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_NPUBBYTES>;
using FileId = std::array<unsigned char, 16>;

struct ExperimentalEnvelope {
  Salt salt{};
  Nonce wrapNonce{};
  Nonce payloadNonce{};
  FileId fileId{};
  QByteArray wrappedKey;
  QByteArray encryptedPayload;
};

QByteArray ad(const char *domain, const FileId &id) {
  QByteArray result(domain);
  result.append(reinterpret_cast<const char *>(id.data()),
                static_cast<qsizetype>(id.size()));
  return result;
}

bool derive(const QByteArray &password, const Salt &salt, Key *key) {
  if (!key || password.isEmpty() || password.size() > 4096)
    return false;
  return crypto_pwhash(
             key->data(), key->size(), password.constData(),
             static_cast<unsigned long long>(password.size()), salt.data(),
             kOps, kMem, crypto_pwhash_ALG_ARGON2ID13) == 0;
}

bool seal(const QByteArray &plain, const QByteArray &password,
          ExperimentalEnvelope *envelope) {
  if (!envelope || plain.size() > 1024 * 1024)
    return false; // Synthetic fixture bound; not the product task limit.
  if (sodium_init() < 0)
    return false;
  *envelope = ExperimentalEnvelope{};
  randombytes_buf(envelope->salt.data(), envelope->salt.size());
  randombytes_buf(envelope->wrapNonce.data(), envelope->wrapNonce.size());
  randombytes_buf(envelope->payloadNonce.data(), envelope->payloadNonce.size());
  randombytes_buf(envelope->fileId.data(), envelope->fileId.size());

  Key kek{}, dek{};
  if (!derive(password, envelope->salt, &kek))
    return false;
  randombytes_buf(dek.data(), dek.size());

  QByteArray wrapAd = ad(kWrapDomain, envelope->fileId);
  envelope->wrappedKey.resize(
      static_cast<qsizetype>(dek.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES));
  unsigned long long written = 0;
  int rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
      reinterpret_cast<unsigned char *>(envelope->wrappedKey.data()),
      &written, dek.data(), dek.size(),
      reinterpret_cast<const unsigned char *>(wrapAd.constData()),
      static_cast<unsigned long long>(wrapAd.size()),
      nullptr, envelope->wrapNonce.data(), kek.data());
  sodium_memzero(kek.data(), kek.size());
  if (rc != 0 || written != static_cast<unsigned long long>(envelope->wrappedKey.size())) {
    sodium_memzero(dek.data(), dek.size());
    *envelope = ExperimentalEnvelope{};
    return false;
  }

  QByteArray dataAd = ad(kDataDomain, envelope->fileId);
  envelope->encryptedPayload.resize(
      plain.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES);
  rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
      reinterpret_cast<unsigned char *>(envelope->encryptedPayload.data()),
      &written, reinterpret_cast<const unsigned char *>(plain.constData()),
      static_cast<unsigned long long>(plain.size()),
      reinterpret_cast<const unsigned char *>(dataAd.constData()),
      static_cast<unsigned long long>(dataAd.size()),
      nullptr, envelope->payloadNonce.data(), dek.data());
  sodium_memzero(dek.data(), dek.size());
  if (rc != 0 || written != static_cast<unsigned long long>(envelope->encryptedPayload.size())) {
    *envelope = ExperimentalEnvelope{};
    return false;
  }
  return true;
}

bool open(const ExperimentalEnvelope &envelope, const QByteArray &password,
          QByteArray *plain) {
  if (!plain)
    return false;
  plain->clear();
  if (sodium_init() < 0 ||
      envelope.wrappedKey.size() !=
          crypto_aead_xchacha20poly1305_ietf_KEYBYTES +
              crypto_aead_xchacha20poly1305_ietf_ABYTES ||
      envelope.encryptedPayload.size() < crypto_aead_xchacha20poly1305_ietf_ABYTES ||
      envelope.encryptedPayload.size() > 1024 * 1024 +
          crypto_aead_xchacha20poly1305_ietf_ABYTES)
    return false;

  Key kek{}, dek{};
  if (!derive(password, envelope.salt, &kek))
    return false;
  QByteArray wrapAd = ad(kWrapDomain, envelope.fileId);
  unsigned long long openedLength = 0;
  int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
      dek.data(), &openedLength, nullptr,
      reinterpret_cast<const unsigned char *>(envelope.wrappedKey.constData()),
      static_cast<unsigned long long>(envelope.wrappedKey.size()),
      reinterpret_cast<const unsigned char *>(wrapAd.constData()),
      static_cast<unsigned long long>(wrapAd.size()),
      envelope.wrapNonce.data(), kek.data());
  sodium_memzero(kek.data(), kek.size());
  if (rc != 0 || openedLength != dek.size()) {
    sodium_memzero(dek.data(), dek.size());
    return false;
  }

  QByteArray staged;
  staged.resize(envelope.encryptedPayload.size() -
                crypto_aead_xchacha20poly1305_ietf_ABYTES);
  QByteArray dataAd = ad(kDataDomain, envelope.fileId);
  rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
      reinterpret_cast<unsigned char *>(staged.data()), &openedLength,
      nullptr,
      reinterpret_cast<const unsigned char *>(envelope.encryptedPayload.constData()),
      static_cast<unsigned long long>(envelope.encryptedPayload.size()),
      reinterpret_cast<const unsigned char *>(dataAd.constData()),
      static_cast<unsigned long long>(dataAd.size()),
      envelope.payloadNonce.data(), dek.data());
  sodium_memzero(dek.data(), dek.size());
  if (rc != 0 || openedLength != static_cast<unsigned long long>(staged.size())) {
    if (!staged.isEmpty())
      sodium_memzero(staged.data(), static_cast<size_t>(staged.size()));
    return false;
  }
  *plain = std::move(staged);
  return true;
}
} // namespace

class TestPortableCryptoProbe : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { QVERIFY(sodium_init() >= 0); }

  void sealedRoundTrip() {
    const QByteArray password("synthetic-ONLY-password");
    const QByteArray data("synthetic-cloud:test\0a\0b", 24);
    ExperimentalEnvelope e;
    QVERIFY(seal(data, password, &e));
    QVERIFY(!e.encryptedPayload.contains(data));
    QVERIFY(!e.wrappedKey.contains(data));
    QByteArray restored;
    QVERIFY(open(e, password, &restored));
    QCOMPARE(restored, data);
  }

  void incorrectPasswordFailsClosed() {
    ExperimentalEnvelope e;
    QVERIFY(seal(QByteArray("fake-task"), QByteArray("test-password-A"), &e));
    QByteArray output("previous-private-data");
    QVERIFY(!open(e, QByteArray("test-password-B"), &output));
    QVERIFY(output.isEmpty());
  }

  void tamperedPayloadFailsClosed() {
    ExperimentalEnvelope e;
    QVERIFY(seal(QByteArray("fake-task"), QByteArray("test-password"), &e));
    e.encryptedPayload[e.encryptedPayload.size() - 1] =
        char(e.encryptedPayload.at(e.encryptedPayload.size() - 1) ^ 1);
    QByteArray output("previous-private-data");
    QVERIFY(!open(e, QByteArray("test-password"), &output));
    QVERIFY(output.isEmpty());
  }

  void tamperedWrappedKeyFailsClosed() {
    ExperimentalEnvelope e;
    QVERIFY(seal(QByteArray("fake-task"), QByteArray("test-password"), &e));
    e.wrappedKey[0] = char(e.wrappedKey.at(0) ^ 1);
    QByteArray output("previous-private-data");
    QVERIFY(!open(e, QByteArray("test-password"), &output));
    QVERIFY(output.isEmpty());
  }

  void tamperedFileIdFailsClosed() {
    ExperimentalEnvelope e;
    QVERIFY(seal(QByteArray("fake-task"), QByteArray("test-password"), &e));
    e.fileId[0] ^= 1;
    QByteArray output;
    QVERIFY(!open(e, QByteArray("test-password"), &output));
    QVERIFY(output.isEmpty());
  }

  void wrongSaltFailsClosed() {
    ExperimentalEnvelope e;
    QVERIFY(seal(QByteArray("fake-task"), QByteArray("test-password"), &e));
    e.salt[0] ^= 1;
    QByteArray output;
    QVERIFY(!open(e, QByteArray("test-password"), &output));
    QVERIFY(output.isEmpty());
  }

  void payloadAndWrappingNoncesAreRandomized() {
    ExperimentalEnvelope a, b;
    const QByteArray pw("test-password");
    QVERIFY(seal(QByteArray("same synthetic payload"), pw, &a));
    QVERIFY(seal(QByteArray("same synthetic payload"), pw, &b));
    QVERIFY(a.fileId != b.fileId);
    QVERIFY(a.wrapNonce != b.wrapNonce);
    QVERIFY(a.payloadNonce != b.payloadNonce);
    QVERIFY(a.encryptedPayload != b.encryptedPayload);
  }

  void emptyPayloadIsAuthenticated() {
    ExperimentalEnvelope e;
    QVERIFY(seal(QByteArray(), QByteArray("test-password"), &e));
    QByteArray output("previous");
    QVERIFY(open(e, QByteArray("test-password"), &output));
    QVERIFY(output.isEmpty());
  }
};

QTEST_GUILESS_MAIN(TestPortableCryptoProbe)
#include "test_portable_crypto_probe.moc"
