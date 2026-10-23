// EXPERIMENTAL wire-format candidate ONLY. Never linked into the app.
// No on-disk tasks.bin migration; uses synthetic test strings exclusively.
// Cryptography is provided by libsodium, not implemented here.
#include <QtTest>
#include <QByteArray>
#include <QCryptographicHash>
#include <QFile>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QString>

#include <array>
#include <cstdint>
#include <limits>
#include <utility>
#include <sodium.h>

namespace {
constexpr int kHeader = 106;
constexpr int kWrapped = 32 + crypto_aead_xchacha20poly1305_ietf_ABYTES;
constexpr int kTag = crypto_aead_xchacha20poly1305_ietf_ABYTES;
constexpr int kMaxPlain = 1024 * 1024; // fixture-only; NOT product policy
constexpr quint32 kOps = static_cast<quint32>(crypto_pwhash_OPSLIMIT_INTERACTIVE);
constexpr quint32 kMemKiB = static_cast<quint32>(crypto_pwhash_MEMLIMIT_INTERACTIVE / 1024);
// These bytes are a versioned *test-fixture* profile, never a production
// password policy. Upgrading libsodium must not silently change this format.
static_assert(kOps == 2u && kMemKiB == 65536u,
              "Draft v2 test vector KDF parameters must remain stable");
constexpr int kTotalMax = kHeader + kWrapped + kMaxPlain + kTag;
constexpr char kMagic[] = "RBTSENC2";
constexpr char kWrapDomain[] = "RcloneBrowser/RBTSENC2/wrap/1";
constexpr char kDataDomain[] = "RcloneBrowser/RBTSENC2/payload/1";
static_assert(crypto_pwhash_SALTBYTES == 16, "v2 draft assumes 16-byte salt");
static_assert(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES == 24,
              "v2 draft assumes 24-byte nonces");
static_assert(kWrapped == 48, "v2 draft assumes 48-byte DEK wrapper");

using Key = std::array<unsigned char, 32>;
using Salt = std::array<unsigned char, 16>;
using FileId = std::array<unsigned char, 16>;
using Nonce = std::array<unsigned char, 24>;

// Research-only instrumentation: an adversarial header MUST be rejected
// before the 64-MiB Argon2id call (or a future more costly KDF).
unsigned int kdfCalls = 0;

struct Inputs { // Allows only test fixtures to choose deterministic randomness.
  FileId fileId{};
  Salt salt{};
  Nonce wrapNonce{};
  Nonce payloadNonce{};
  Key dek{};
};

void appendU16(QByteArray *b, quint16 n) {
  b->append(char((n >> 8) & 255));
  b->append(char(n & 255));
}
void appendU32(QByteArray *b, quint32 n) {
  for (int shift : {24, 16, 8, 0})
    b->append(char((n >> shift) & 255));
}
quint16 readU16(const QByteArray &b, int off) {
  return quint16((quint16(quint8(b.at(off))) << 8) |
                  quint16(quint8(b.at(off + 1))));
}
quint32 readU32(const QByteArray &b, int off) {
  quint32 n = 0;
  for (int i = 0; i < 4; ++i)
    n = (n << 8) | quint8(b.at(off + i));
  return n;
}
template <std::size_t N>
void appendBytes(QByteArray *b, const std::array<unsigned char, N> &bytes) {
  b->append(reinterpret_cast<const char *>(bytes.data()), int(bytes.size()));
}
QByteArray associatedData(const char *domain, const QByteArray &header,
                          const QByteArray &wrapped = QByteArray()) {
  QByteArray result(domain);
  result.append(header);
  result.append(wrapped);
  return result;
}
bool derive(const QByteArray &password, const Salt &salt, Key *key) {
  if (!key || password.isEmpty() || password.size() > 4096)
    return false;
  ++kdfCalls;
  return crypto_pwhash(
      key->data(), key->size(), password.constData(),
      static_cast<unsigned long long>(password.size()), salt.data(),
      kOps, static_cast<size_t>(kMemKiB) * 1024,
      crypto_pwhash_ALG_ARGON2ID13) == 0;
}
Inputs randomInputs() {
  Inputs x;
  randombytes_buf(x.fileId.data(), x.fileId.size());
  randombytes_buf(x.salt.data(), x.salt.size());
  randombytes_buf(x.wrapNonce.data(), x.wrapNonce.size());
  randombytes_buf(x.payloadNonce.data(), x.payloadNonce.size());
  randombytes_buf(x.dek.data(), x.dek.size());
  return x;
}
Inputs fixedInputs() {
  Inputs x;
  x.fileId.fill(0x11);
  x.salt.fill(0x22);
  x.wrapNonce.fill(0x33);
  x.payloadNonce.fill(0x44);
  x.dek.fill(0x55);
  return x;
}
QByteArray makeHeader(const Inputs &inputs, int cipherSize) {
  QByteArray h(kMagic, 8);
  h.append(char(1)); // experimental minor version
  h.append(char(1)); // XChaCha20-Poly1305 IETF
  h.append(char(1)); // Argon2id13
  h.append(char(1)); // one password slot; DPAPI is NOT supported here
  appendU32(&h, kOps);
  appendU32(&h, kMemKiB);
  appendBytes(&h, inputs.fileId);
  appendBytes(&h, inputs.salt);
  appendBytes(&h, inputs.wrapNonce);
  appendBytes(&h, inputs.payloadNonce);
  appendU16(&h, kWrapped);
  appendU32(&h, quint32(cipherSize));
  return h;
}

// Creates an in-memory synthetic envelope. No filesystem writes.
bool seal(const QByteArray &plain, const QByteArray &password, QByteArray *out,
          const Inputs *fixture = nullptr) {
  // The API must not destroy input when the caller accidentally aliases
  // its output buffer with the plaintext buffer.
  if (!out || out == &plain)
    return false;
  out->clear();
  if (sodium_init() < 0 || plain.size() > kMaxPlain ||
      password.isEmpty() || password.size() > 4096)
    return false;

  Inputs x = fixture ? *fixture : randomInputs();
  Key kek{};
  if (!derive(password, x.salt, &kek)) {
    sodium_memzero(x.dek.data(), x.dek.size());
    return false;
  }
  const QByteArray header = makeHeader(x, int(plain.size()) + kTag);
  if (header.size() != kHeader) {
    sodium_memzero(kek.data(), kek.size());
    sodium_memzero(x.dek.data(), x.dek.size());
    return false;
  }
  QByteArray wrapped;
  wrapped.resize(kWrapped);
  unsigned long long written = 0;
  const QByteArray wad = associatedData(kWrapDomain, header);
  int rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
      reinterpret_cast<unsigned char *>(wrapped.data()), &written,
      x.dek.data(), x.dek.size(),
      reinterpret_cast<const unsigned char *>(wad.constData()),
      static_cast<unsigned long long>(wad.size()), nullptr,
      x.wrapNonce.data(), kek.data());
  sodium_memzero(kek.data(), kek.size());
  if (rc != 0 || written != static_cast<unsigned long long>(kWrapped)) {
    sodium_memzero(x.dek.data(), x.dek.size());
    return false;
  }
  QByteArray ciphertext;
  ciphertext.resize(plain.size() + kTag);
  const QByteArray pad = associatedData(kDataDomain, header, wrapped);
  rc = crypto_aead_xchacha20poly1305_ietf_encrypt(
      reinterpret_cast<unsigned char *>(ciphertext.data()), &written,
      reinterpret_cast<const unsigned char *>(plain.constData()),
      static_cast<unsigned long long>(plain.size()),
      reinterpret_cast<const unsigned char *>(pad.constData()),
      static_cast<unsigned long long>(pad.size()), nullptr,
      x.payloadNonce.data(), x.dek.data());
  sodium_memzero(x.dek.data(), x.dek.size());
  if (rc != 0 || written != static_cast<unsigned long long>(ciphertext.size()))
    return false;
  out->reserve(header.size() + wrapped.size() + ciphertext.size());
  out->append(header);
  out->append(wrapped);
  out->append(ciphertext);
  return true;
}

// Explicit parser checks all attacker-supplied lengths / KDF costs BEFORE
// any key derivation, allocations proportional to the declared length, or AEAD.
bool open(const QByteArray &wire, const QByteArray &password, QByteArray *plain) {
  // Never wipe the stored ciphertext if the caller passes its address as
  // the output parameter. Such a programming error must be fail-closed.
  if (!plain || plain == &wire)
    return false;
  plain->clear();
  if (sodium_init() < 0 || password.isEmpty() || password.size() > 4096 ||
      wire.size() < kHeader + kWrapped + kTag || wire.size() > kTotalMax)
    return false;
  // Fail-closed authorization precedes *any* Argon2id operation. Header
  // parameters are attacker-controlled until the AEAD tags are verified.
  // The experimental revision 1 intentionally authorizes one fixed
  // research profile; it must NEVER auto-accept a weaker/stronger setting
  // just because the value is valid according to the libsodium API.
  const quint32 fileOps = readU32(wire, 12);
  const quint32 fileMemKiB = readU32(wire, 16);
  if (wire.left(8) != QByteArray(kMagic, 8) ||
      quint8(wire.at(8)) != 1 || quint8(wire.at(9)) != 1 ||
      quint8(wire.at(10)) != 1 || quint8(wire.at(11)) != 1 ||
      fileOps != 2u || fileMemKiB != 65536u ||
      readU16(wire, 100) != kWrapped)
    return false;
  const quint32 cipherSize = readU32(wire, 102);
  if (cipherSize < quint32(kTag) ||
      cipherSize > quint32(kMaxPlain + kTag) ||
      wire.size() != kHeader + kWrapped + qsizetype(cipherSize))
    return false;

  Salt salt{};
  Nonce wrapNonce{}, payloadNonce{};
  for (int i = 0; i < 16; ++i)
    salt[size_t(i)] = static_cast<unsigned char>(wire.at(36 + i));
  for (int i = 0; i < 24; ++i) {
    wrapNonce[size_t(i)] = static_cast<unsigned char>(wire.at(52 + i));
    payloadNonce[size_t(i)] = static_cast<unsigned char>(wire.at(76 + i));
  }
  const QByteArray header = wire.left(kHeader);
  const QByteArray wrapped = wire.mid(kHeader, kWrapped);

  Key kek{}, dek{};
  if (!derive(password, salt, &kek))
    return false;
  unsigned long long opened = 0;
  const QByteArray wad = associatedData(kWrapDomain, header);
  int rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
      dek.data(), &opened, nullptr,
      reinterpret_cast<const unsigned char *>(wrapped.constData()), kWrapped,
      reinterpret_cast<const unsigned char *>(wad.constData()),
      static_cast<unsigned long long>(wad.size()),
      wrapNonce.data(), kek.data());
  sodium_memzero(kek.data(), kek.size());
  if (rc != 0 || opened != dek.size()) {
    sodium_memzero(dek.data(), dek.size());
    return false;
  }

  QByteArray staged;
  staged.resize(qsizetype(cipherSize) - kTag);
  const QByteArray pad = associatedData(kDataDomain, header, wrapped);
  rc = crypto_aead_xchacha20poly1305_ietf_decrypt(
      reinterpret_cast<unsigned char *>(staged.data()), &opened, nullptr,
      reinterpret_cast<const unsigned char *>(wire.constData() + kHeader + kWrapped),
      static_cast<unsigned long long>(cipherSize),
      reinterpret_cast<const unsigned char *>(pad.constData()),
      static_cast<unsigned long long>(pad.size()),
      payloadNonce.data(), dek.data());
  sodium_memzero(dek.data(), dek.size());
  if (rc != 0 || opened != static_cast<unsigned long long>(staged.size())) {
    if (!staged.isEmpty())
      sodium_memzero(staged.data(), static_cast<size_t>(staged.size()));
    return false;
  }
  *plain = std::move(staged);
  return true;
}
} // namespace

class TestPortableV2Envelope : public QObject {
  Q_OBJECT
  const QByteArray pw = QByteArray("synthetic-fixture-password");
  const QByteArray payload = QByteArray("Synthetic portable task bytes v2");

  void rejectWithClearedOutput(const QByteArray &wire,
                               const QByteArray &pass) {
    QByteArray output("old private content");
    QVERIFY(!open(wire, pass, &output));
    QVERIFY(output.isEmpty());
  }

private slots:
  void initTestCase() { QVERIFY(sodium_init() >= 0); }

  void canonicalLayoutAndByteOrdering() {
    QByteArray wire;
    const Inputs x = fixedInputs();
    QVERIFY(seal(payload, pw, &wire, &x));
    QCOMPARE(wire.size(), kHeader + kWrapped + payload.size() + kTag);
    QCOMPARE(wire.left(12).toHex(), QByteArray("52425453454e433201010101"));
    QCOMPARE(readU32(wire, 12), kOps);
    QCOMPARE(readU32(wire, 16), kMemKiB);
    QCOMPARE(wire.mid(20, 16), QByteArray(16, char(0x11)));
    QCOMPARE(wire.mid(36, 16), QByteArray(16, char(0x22)));
    QCOMPARE(wire.mid(52, 24), QByteArray(24, char(0x33)));
    QCOMPARE(wire.mid(76, 24), QByteArray(24, char(0x44)));
    QCOMPARE(readU16(wire, 100), quint16(kWrapped));
    QCOMPARE(readU32(wire, 102), quint32(payload.size() + kTag));
    QByteArray decoded;
    QVERIFY(open(wire, pw, &decoded));
    QCOMPARE(decoded, payload);
  }

  void deterministicCrossPlatformFixture() {
    const Inputs x = fixedInputs();
    QByteArray a, b;
    QVERIFY(seal(payload, pw, &a, &x));
    QVERIFY(seal(payload, pw, &b, &x));
    QCOMPARE(a, b);
    // Independently generated using libsodium 1.0.18 and Python ctypes.
    // This fixed-input vector is synthetic and must never be used for real
    // encryption; both Windows and Linux must produce these exact bytes.
    QCOMPARE(QCryptographicHash::hash(a, QCryptographicHash::Sha256).toHex(),
             QByteArray("6a166dd34aeb98928517b293975dd70be4635eb7271f75c6644acd2769159674"));
  }

  void emptyPayloadRoundTrips() {
    QByteArray wire, recovered("old");
    QVERIFY(seal({}, pw, &wire));
    QVERIFY(open(wire, pw, &recovered));
    QVERIFY(recovered.isEmpty());
    QCOMPARE(wire.size(), kHeader + kWrapped + kTag);
  }
  void wrongPasswordFailsClosed() {
    QByteArray wire;
    QVERIFY(seal(payload, pw, &wire));
    rejectWithClearedOutput(wire, QByteArray("wrong password"));
  }
  void metadataTamperFailsClosed() {
    QByteArray wire;
    QVERIFY(seal(payload, pw, &wire));
    // Includes metadata, KDF settings, file ID, all nonces, length fields.
    for (int offset : {8, 9, 10, 11, 12, 15, 16, 19, 20, 35,
                       36, 51, 52, 75, 76, 99, 100, 101, 102, 105}) {
      QByteArray damaged(wire);
      damaged[offset] = char(quint8(damaged.at(offset)) ^ 1);
      rejectWithClearedOutput(damaged, pw);
    }
  }
  void ciphertextTamperFailsClosed() {
    QByteArray wire;
    QVERIFY(seal(payload, pw, &wire));
    for (int offset : {kHeader, kHeader + kWrapped - 1,
                       kHeader + kWrapped, int(wire.size()) - 1}) {
      QByteArray damaged(wire);
      damaged[offset] = char(quint8(damaged.at(offset)) ^ 1);
      rejectWithClearedOutput(damaged, pw);
    }
  }
  void truncationAndTrailingBytesFailClosed() {
    QByteArray wire;
    QVERIFY(seal(payload, pw, &wire));
    for (int len : {0, 4, 8, kHeader - 1, kHeader, kHeader + kWrapped - 1,
                    int(wire.size()) - 1}) {
      rejectWithClearedOutput(wire.left(len), pw);
    }
    rejectWithClearedOutput(wire + "extra", pw);
  }
  void hostileHeadersNeverInvokeArgon2id() {
    QByteArray wire;
    QVERIFY(seal(payload, pw, &wire));
    // Every case is rejected during structural/KDF policy validation
    // without even starting a potentially costly password hash.
    const unsigned int callsBefore = kdfCalls;
    for (int offset : {0, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
                       18, 19, 100, 101, 102, 103, 104, 105}) {
      QByteArray damaged = wire;
      damaged[offset] = char(quint8(damaged.at(offset)) ^ 1u);
      rejectWithClearedOutput(damaged, pw);
      QCOMPARE(kdfCalls, callsBefore);
    }
    // Plausible-but-UNAPPROVED profiles must also be refused rather
    // than silently downgrading or consuming attacker-controlled RAM.
    for (const auto &candidate : {
           std::pair<quint32, quint32>{1u, 65536u},
           {3u, 65536u}, {2u, 32768u}, {2u, 262144u},
           {0xffffffffu, 0xffffffffu}, {0u, 0u}}) {
      QByteArray altered = wire;
      for (int i = 0; i < 4; ++i) {
        altered[12 + i] = char((candidate.first >> (24 - 8 * i)) & 0xff);
        altered[16 + i] = char((candidate.second >> (24 - 8 * i)) & 0xff);
      }
      rejectWithClearedOutput(altered, pw);
      QCOMPARE(kdfCalls, callsBefore);
    }
    rejectWithClearedOutput(wire.left(kHeader - 1), pw);
    QCOMPARE(kdfCalls, callsBefore);
    rejectWithClearedOutput(wire + "surplus", pw);
    QCOMPARE(kdfCalls, callsBefore);
    rejectWithClearedOutput(QByteArray(kTotalMax + 1, char(0)), pw);
    QCOMPARE(kdfCalls, callsBefore);
  }

  void authorizedHeaderDerivesOnlyAfterValidation() {
    QByteArray wire;
    QVERIFY(seal(payload, pw, &wire));
    const unsigned int before = kdfCalls;
    QByteArray decoded;
    QVERIFY(open(wire, pw, &decoded));
    QCOMPARE(decoded, payload);
    QCOMPARE(kdfCalls, before + 1);
  }

  void oversizedCiphertextAndKdfParametersFailEarly() {
    QByteArray wire;
    QVERIFY(seal(payload, pw, &wire));
    // No giant allocation or attacker-requested high Argon2 resources.
    QByteArray damaged(wire);
    for (int i = 102; i < 106; ++i) damaged[i] = char(0xff);
    rejectWithClearedOutput(damaged, pw);
    damaged = wire;
    damaged[12] = char(0xff);
    rejectWithClearedOutput(damaged, pw);
    damaged = wire;
    damaged[16] = char(0xff);
    rejectWithClearedOutput(damaged, pw);
    rejectWithClearedOutput(QByteArray(kTotalMax + 1, char(0)), pw);
  }
  void swappedPasswordSlotAndPayloadFailClosed() {
    QByteArray a, b;
    QVERIFY(seal(payload, pw, &a));
    QVERIFY(seal(payload, pw, &b));
    QByteArray swapped(a);
    swapped.replace(kHeader, kWrapped, b.mid(kHeader, kWrapped));
    rejectWithClearedOutput(swapped, pw);
    swapped = a;
    swapped.replace(kHeader + kWrapped, a.size() - kHeader - kWrapped,
                    b.mid(kHeader + kWrapped));
    rejectWithClearedOutput(swapped, pw);
  }
  void randomizedEncryptionsDiffer() {
    QByteArray a, b, restored;
    QVERIFY(seal(payload, pw, &a));
    QVERIFY(seal(payload, pw, &b));
    QVERIFY(a != b);
    QVERIFY(open(a, pw, &restored));
    QCOMPARE(restored, payload);
    QVERIFY(open(b, pw, &restored));
    QCOMPARE(restored, payload);
  }
  void aliasedInputOutputNeverDestroysProtectedData() {
    QByteArray encrypted;
    QVERIFY(seal(payload, pw, &encrypted));
    const QByteArray snapshot = encrypted;

    // An accidental in-place read or write must return false and keep the
    // source intact, even if the main password is wrong.
    QVERIFY(!open(encrypted, pw, &encrypted));
    QCOMPARE(encrypted, snapshot);
    QVERIFY(!open(encrypted, QByteArray("wrong password"), &encrypted));
    QCOMPARE(encrypted, snapshot);

    QByteArray syntheticPlain(payload);
    QVERIFY(!seal(syntheticPlain, pw, &syntheticPlain));
    QCOMPARE(syntheticPlain, payload);

    QByteArray decoded;
    QVERIFY(open(encrypted, pw, &decoded));
    QCOMPARE(decoded, payload);
  }

  void corruptedEncryptedDiskFixtureRemainsUnchangedOnFailure() {
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());
    const QString path = sandbox.filePath("synthetic-corrupted-protected-file");

    QByteArray encrypted;
    QVERIFY(seal(payload, pw, &encrypted));
    // Change an authenticated payload byte, then simulate reading it from
    // disk. No real task file and no production storage code is accessed.
    encrypted[encrypted.size() - 1] =
        char(quint8(encrypted.at(encrypted.size() - 1)) ^ 1u);
    {
      QSaveFile writer(path);
      writer.setDirectWriteFallback(false);
      QVERIFY(writer.open(QIODevice::WriteOnly));
      QCOMPARE(writer.write(encrypted), qint64(encrypted.size()));
      QVERIFY(writer.commit());
    }
    {
      QFile reader(path);
      QVERIFY(reader.open(QIODevice::ReadOnly));
      const QByteArray stored = reader.read(kTotalMax + 1);
      QCOMPARE(reader.error(), QFileDevice::NoError);
      QVERIFY(reader.atEnd());
      QCOMPARE(stored, encrypted);

      QByteArray decoded("stale plaintext");
      QVERIFY(!open(stored, pw, &decoded));
      QVERIFY(decoded.isEmpty());
    }
    QFile unchanged(path);
    QVERIFY(unchanged.open(QIODevice::ReadOnly));
    QCOMPARE(unchanged.readAll(), encrypted);
  }

  void encryptedSyntheticFileSurvivesPathMoveAndReload() {
    // This is a disposable fixture in a private temporary directory.
    // No real tasks.bin, user data, or application storage is involved.
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());
    const QString initialPath = sandbox.filePath("synthetic-protected-fixture");
    const QString relocatedPath = sandbox.filePath("relocated-protected-fixture");

    QByteArray encrypted;
    QVERIFY(seal(payload, pw, &encrypted));
    QVERIFY(!encrypted.contains(payload));
    {
      QSaveFile writer(initialPath);
      writer.setDirectWriteFallback(false);
      QVERIFY(writer.open(QIODevice::WriteOnly));
      QCOMPARE(writer.write(encrypted), qint64(encrypted.size()));
      QVERIFY(writer.commit());
    }
    QVERIFY(QFile::rename(initialPath, relocatedPath));
    QByteArray reloaded;
    {
      QFile reader(relocatedPath);
      QVERIFY(reader.open(QIODevice::ReadOnly));
      // A bounded read is essential for hostile files; this fixture uses
      // a 1-MiB cap rather than the as-yet undecided production limit.
      reloaded = reader.read(kTotalMax + 1);
      QVERIFY(reader.atEnd());
      QCOMPARE(reader.error(), QFileDevice::NoError);
    }
    QCOMPARE(reloaded, encrypted);

    // A new read from disk, independent of the original encrypted buffer,
    // must recover with the password, without Windows DPAPI or path binding.
    QByteArray recovered("old staged contents");
    QVERIFY(open(reloaded, pw, &recovered));
    QCOMPARE(recovered, payload);
    rejectWithClearedOutput(reloaded, QByteArray("synthetic-wrong-password"));

    // Failure to unlock must never rewrite or discard the encrypted file.
    QFile unchanged(relocatedPath);
    QVERIFY(unchanged.open(QIODevice::ReadOnly));
    QCOMPARE(unchanged.readAll(), encrypted);
  }

  void cancelledSyntheticRewritePreservesProtectedFile() {
    QTemporaryDir sandbox;
    QVERIFY(sandbox.isValid());
    const QString path = sandbox.filePath("never-a-real-tasks-file");
    QByteArray original;
    QVERIFY(seal(payload, pw, &original));
    {
      QSaveFile writer(path);
      writer.setDirectWriteFallback(false);
      QVERIFY(writer.open(QIODevice::WriteOnly));
      QCOMPARE(writer.write(original), qint64(original.size()));
      QVERIFY(writer.commit());
    }
    {
      QSaveFile attemptedOverwrite(path);
      attemptedOverwrite.setDirectWriteFallback(false);
      QVERIFY(attemptedOverwrite.open(QIODevice::WriteOnly));
      const QByteArray incomplete("incomplete-synthetic-contents");
      QCOMPARE(attemptedOverwrite.write(incomplete),
               qint64(incomplete.size()));
      attemptedOverwrite.cancelWriting();
    }
    QFile existing(path);
    QVERIFY(existing.open(QIODevice::ReadOnly));
    const QByteArray retained = existing.readAll();
    QCOMPARE(retained, original);
    QByteArray recovered;
    QVERIFY(open(retained, pw, &recovered));
    QCOMPARE(recovered, payload);
  }

  void unicodePasswordBytesExposeUnresolvedNormalizationPolicy() {
    // NFC composed and decomposed spellings look the same but have
    // different UTF-8 encodings. The research codec takes raw bytes,
    // without any GUI or Unicode normalization contract yet.
    const QByteArray composed = QByteArray::fromHex("436166c3a9");
    const QByteArray decomposed = QByteArray::fromHex("43616665cc81");
    QVERIFY(composed != decomposed);
    const QString visuallySameA = QString::fromUtf8(composed);
    const QString visuallySameB = QString::fromUtf8(decomposed);
    QCOMPARE(visuallySameA.normalized(QString::NormalizationForm_C),
             visuallySameB.normalized(QString::NormalizationForm_C));
    QByteArray encrypted;
    QVERIFY(seal(payload, composed, &encrypted));
    QByteArray recovered;
    QVERIFY(open(encrypted, composed, &recovered));
    QCOMPARE(recovered, payload);
    rejectWithClearedOutput(encrypted, decomposed);

    // This does NOT approve raw-byte passwords for the GUI. It proves
    // normalization must be a conscious, versioned product decision
    // before real credentials and backups are ever encrypted.
  }

  void unsupportedMagicAndExtraSlotsFailClosed() {
    QByteArray wire;
    QVERIFY(seal(payload, pw, &wire));
    QByteArray unknown = wire;
    unknown.replace(0, 8, QByteArray("RBTSENC1", 8));
    rejectWithClearedOutput(unknown, pw);
    unknown = wire;
    unknown[11] = char(2); // DPAPI slot not implemented yet.
    rejectWithClearedOutput(unknown, pw);
  }
};
QTEST_GUILESS_MAIN(TestPortableV2Envelope)
#include "test_portable_v2_envelope.moc"
