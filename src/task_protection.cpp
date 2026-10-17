#include "task_protection.h"

#include <QByteArray>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <wincrypt.h>
#include <limits>
#endif

namespace TaskProtection {
namespace {
const QByteArray kEnvelopeMagic = QByteArrayLiteral("RBTSENC1");
const QByteArray kMagicPrefix = QByteArrayLiteral("RBTSENC");
const QByteArray kInnerMarker = QByteArrayLiteral("RBTaskPayload/1:");

bool fail(QString *error, const QString &message) {
  if (error) *error = message;
  return false;
}

#ifdef Q_OS_WIN
// Non-secret optional entropy binds this encrypted blob to its specific
// application purpose; DPAPI owns key management and integrity protection.
BYTE kDpapiEntropy[] = "RcloneBrowser.SavedTasks.DPAPI.v1";

DATA_BLOB entropy() {
  DATA_BLOB e{};
  e.cbData = static_cast<DWORD>(sizeof(kDpapiEntropy) - 1);
  e.pbData = kDpapiEntropy;
  return e;
}

void zeroMemory(QByteArray *bytes) {
  if (bytes && !bytes->isEmpty())
    SecureZeroMemory(bytes->data(), static_cast<SIZE_T>(bytes->size()));
}
#endif
} // namespace

Format inspectFormat(const QByteArray &stored) {
  if (stored.startsWith(kEnvelopeMagic)) return Format::WindowsDpapiV1;
  if (stored.startsWith(kMagicPrefix)) return Format::UnsupportedProtected;
  return Format::LegacyPlaintext;
}

bool encryptForWindowsUser(const QByteArray &plaintext, QByteArray *stored,
                           QString *error) {
  if (!stored) return fail(error, QStringLiteral("Invalid output buffer."));
  stored->clear();
#ifndef Q_OS_WIN
  Q_UNUSED(plaintext);
  return fail(error, QStringLiteral(
      "Windows DPAPI task protection is unavailable on this platform."));
#else
  QByteArray framed = kInnerMarker + plaintext;
  if (static_cast<quint64>(framed.size()) >
      std::numeric_limits<DWORD>::max()) {
    zeroMemory(&framed);
    return fail(error, QStringLiteral("Task data exceeds the protection limit."));
  }
  DATA_BLOB input{};
  input.cbData = static_cast<DWORD>(framed.size());
  input.pbData = reinterpret_cast<BYTE *>(framed.data());
  DATA_BLOB output{};
  DATA_BLOB extra = entropy();
  const BOOL ok =
      CryptProtectData(&input, L"RcloneBrowser saved tasks", &extra,
                       nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
  zeroMemory(&framed);
  if (!ok || !output.pbData || !output.cbData) {
    if (output.pbData) {
      SecureZeroMemory(output.pbData, output.cbData);
      LocalFree(output.pbData);
    }
    return fail(error, QStringLiteral("Windows could not protect saved tasks."));
  }
  *stored = kEnvelopeMagic +
            QByteArray(reinterpret_cast<const char *>(output.pbData),
                       static_cast<qsizetype>(output.cbData));
  SecureZeroMemory(output.pbData, output.cbData);
  LocalFree(output.pbData);
  if (error) error->clear();
  return true;
#endif
}

bool decryptForWindowsUser(const QByteArray &stored, QByteArray *plaintext,
                           QString *error) {
  if (!plaintext) return fail(error, QStringLiteral("Invalid output buffer."));
  plaintext->clear();
  if (inspectFormat(stored) != Format::WindowsDpapiV1 ||
      stored.size() <= kEnvelopeMagic.size()) {
    return fail(error, QStringLiteral(
        "Saved task envelope is missing, unsupported or incomplete."));
  }
#ifndef Q_OS_WIN
  return fail(error, QStringLiteral(
      "Windows-protected tasks cannot be opened on this platform."));
#else
  const QByteArray ciphertext = stored.mid(kEnvelopeMagic.size());
  if (static_cast<quint64>(ciphertext.size()) >
      std::numeric_limits<DWORD>::max()) {
    return fail(error, QStringLiteral("Task data exceeds the protection limit."));
  }
  DATA_BLOB input{};
  input.cbData = static_cast<DWORD>(ciphertext.size());
  // Windows' DPAPI API does not modify the supplied encrypted input.
  input.pbData =
      reinterpret_cast<BYTE *>(const_cast<char *>(ciphertext.constData()));
  DATA_BLOB decoded{};
  DATA_BLOB extra = entropy();
  const BOOL ok = CryptUnprotectData(&input, nullptr, &extra, nullptr, nullptr,
                                     CRYPTPROTECT_UI_FORBIDDEN, &decoded);
  if (!ok || !decoded.pbData) {
    if (decoded.pbData) {
      SecureZeroMemory(decoded.pbData, decoded.cbData);
      LocalFree(decoded.pbData);
    }
    return fail(error, QStringLiteral(
        "Unable to decrypt saved tasks for this Windows user."));
  }
  const QByteArray framed(reinterpret_cast<const char *>(decoded.pbData),
                          static_cast<qsizetype>(decoded.cbData));
  SecureZeroMemory(decoded.pbData, decoded.cbData);
  LocalFree(decoded.pbData);
  if (!framed.startsWith(kInnerMarker)) {
    // Do not disclose any decoded data on failed domain/format checks.
    QByteArray toErase = framed;
    zeroMemory(&toErase);
    return fail(error, QStringLiteral("Protected task format is invalid."));
  }
  *plaintext = framed.mid(kInnerMarker.size());
  // Qt's implicit sharing may retain an additional copy; this codec minimizes
  // temporary exposure but cannot guarantee all in-memory copies are wiped.
  if (error) error->clear();
  return true;
#endif
}
} // namespace TaskProtection
