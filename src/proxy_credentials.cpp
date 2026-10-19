#include "proxy_credentials.h"

#include <QByteArray>
#include <QRegularExpression>
#include <QUuid>
#include <QUrl>
#include <string>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincred.h>
#endif

namespace ProxyCredentials {
namespace {
const QString kPrefix = QStringLiteral("RcloneBrowser.Proxy.v1.");

bool keyAllowed(const QString &key) {
  return key == QLatin1String("Settings/http_proxy") ||
         key == QLatin1String("Settings/https_proxy");
}
QString refKey(const QString &key) {
  return key + QStringLiteral("CredentialTarget");
}
bool validTarget(const QString &target) {
  static const QRegularExpression rx(QStringLiteral(
      "^RcloneBrowser\\.Proxy\\.v1\\.[0-9a-f]{8}-[0-9a-f]{4}-"
      "[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$"));
  return rx.match(target).hasMatch();
}
bool fail(QString *error, const QString &msg) {
  if (error) *error = msg;
  return false;
}

#ifdef Q_OS_WIN
bool writeSecret(const QString &target, const QString &value, QString *error) {
  std::wstring name = target.toStdWString();
  QByteArray utf8 = value.toUtf8();
  if (utf8.isEmpty() || utf8.size() > CRED_MAX_CREDENTIAL_BLOB_SIZE) {
    utf8.fill('\0');
    return fail(error, QStringLiteral(
        "Proxy credentials exceed Windows Credential Manager's size limit."));
  }
  CREDENTIALW credential{};
  credential.Type = CRED_TYPE_GENERIC;
  credential.TargetName = name.data();
  credential.CredentialBlob = reinterpret_cast<LPBYTE>(utf8.data());
  credential.CredentialBlobSize = static_cast<DWORD>(utf8.size());
  credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
  const BOOL ok = CredWriteW(&credential, 0);
  const DWORD err = ok ? 0 : GetLastError();
  SecureZeroMemory(utf8.data(), static_cast<SIZE_T>(utf8.size()));
  if (!ok) return fail(error, QStringLiteral(
      "Windows Credential Manager refused the credentials (error %1).").arg(err));
  return true;
}
bool readSecret(const QString &target, QString *value, QString *error) {
  std::wstring name = target.toStdWString();
  PCREDENTIALW credential = nullptr;
  if (!CredReadW(name.c_str(), CRED_TYPE_GENERIC, 0, &credential)) {
    return fail(error, QStringLiteral(
        "Protected proxy credentials are missing or unavailable (error %1).")
        .arg(GetLastError()));
  }
  if (!credential || !credential->CredentialBlob ||
      credential->CredentialBlobSize == 0) {
    if (credential) CredFree(credential);
    return fail(error, QStringLiteral("Protected proxy credentials are empty."));
  }
  QByteArray bytes(reinterpret_cast<const char *>(credential->CredentialBlob),
                   static_cast<qsizetype>(credential->CredentialBlobSize));
  *value = QString::fromUtf8(bytes);
  const bool valid = !value->isEmpty() && value->toUtf8() == bytes;
  bytes.fill('\0');
  CredFree(credential);
  if (!valid) {
    value->clear();
    return fail(error, QStringLiteral("Protected proxy credentials are invalid."));
  }
  return true;
}
void eraseSecret(const QString &target) {
  if (!validTarget(target)) return;
  std::wstring name = target.toStdWString();
  CredDeleteW(name.c_str(), CRED_TYPE_GENERIC, 0);
}
#endif

bool commitSettings(QSettings *s, const QString &key, const QString &publicUrl,
                    const QString &target, QString *error) {
  const QString rkey = refKey(key);
  const bool hadValue = s->contains(key), hadRef = s->contains(rkey);
  const QVariant oldValue = s->value(key), oldRef = s->value(rkey);
  // Write the marker first, so an interrupted change cannot intentionally
  // misinterpret a sanitized URL as a working, unauthenticated proxy.
  if (target.isEmpty()) s->remove(rkey);
  else s->setValue(rkey, target);
  s->setValue(key, publicUrl);
  s->sync();
  if (s->status() == QSettings::NoError) return true;

  if (hadRef) s->setValue(rkey, oldRef);
  else s->remove(rkey);
  if (hadValue) s->setValue(key, oldValue);
  else s->remove(key);
  s->sync(); // best-effort rollback; never remove old credential
  return fail(error, QStringLiteral(
      "Saving proxy settings failed; the original credential was not removed."));
}
} // namespace

bool hasUserInfo(const QString &url) {
  const QUrl p(url, QUrl::StrictMode);
  return p.isValid() && !p.userInfo(QUrl::FullyEncoded).isEmpty();
}
QString withoutUserInfo(const QString &url) {
  const QUrl p(url, QUrl::StrictMode);
  return p.isValid() ? p.toString(QUrl::FullyEncoded | QUrl::RemoveUserInfo)
                     : QString();
}

bool load(QSettings *s, const QString &key, QString *value, QString *error) {
  if (!s || !value || !keyAllowed(key))
    return fail(error, QStringLiteral("Invalid proxy settings request."));
  const QString target = s->value(refKey(key)).toString();
  if (target.isEmpty()) {
    *value = s->value(key).toString();
    return true;
  }
  if (!validTarget(target))
    return fail(error, QStringLiteral("Invalid proxy credential reference."));
#ifdef Q_OS_WIN
  return readSecret(target, value, error);
#else
  return fail(error, QStringLiteral(
      "A Windows-only protected proxy cannot be read here."));
#endif
}

bool save(QSettings *s, const QString &key, const QString &value,
          bool protect, QString *error) {
  if (!s || !keyAllowed(key))
    return fail(error, QStringLiteral("Invalid proxy settings request."));
  const QString oldTarget = s->value(refKey(key)).toString();
  if (!oldTarget.isEmpty() && !validTarget(oldTarget))
    return fail(error, QStringLiteral("Invalid old proxy credential reference."));

  QString oldValue;
  const bool oldReadable = load(s, key, &oldValue);
  // Equal legacy plaintext must still be migrated to Credential Manager.
  // Short-circuit only if the credential is already protected or the
  // value has no userinfo to protect.
  if (oldReadable && oldValue == value &&
      (!protect || !oldTarget.isEmpty() || !hasUserInfo(value)))
    return true;
  // When a saved Credential Manager entry is missing, the preferences dialog
  // can show only the sanitized public URL. Saving that unchanged value must
  // NOT silently discard the credential reference.
  if (!oldReadable && !oldTarget.isEmpty() &&
      value == s->value(key).toString()) {
    return fail(error, QStringLiteral(
        "Saved proxy credentials could not be loaded. Re-enter the full "
        "authenticated proxy URL before replacing this entry."));
  }

  if (!protect) {
    if (!commitSettings(s, key, value, QString(), error)) return false;
#ifdef Q_OS_WIN
    eraseSecret(oldTarget);
#endif
    return true; // portable/non-Windows compatibility: NOT protected at rest
  }

#ifndef Q_OS_WIN
  return fail(error, QStringLiteral(
      "Windows Credential Manager is unavailable on this system."));
#else
  if (!value.isEmpty()) {
    const QUrl p(value, QUrl::StrictMode);
    if (!p.isValid() || p.scheme().isEmpty() || p.host().isEmpty())
      return fail(error, QStringLiteral(
          "The proxy URL is invalid; old settings were preserved."));
  }
  if (!hasUserInfo(value)) {
    if (!commitSettings(s, key, value, QString(), error)) return false;
    eraseSecret(oldTarget);
    return true;
  }

  const QString target = kPrefix +
      QUuid::createUuid().toString(QUuid::WithoutBraces);
  if (!writeSecret(target, value, error)) return false;
  if (!commitSettings(s, key, withoutUserInfo(value), target, error)) {
    // After an unsuccessful rollback, keeping an orphan secret is preferable
    // to deleting a credential that a partially persisted reference may use.
    return false;
  }
  eraseSecret(oldTarget);
  return true;
#endif
}

bool migrateLegacy(QSettings *s, const QString &key, bool protect,
                   QString *error) {
  if (!s || !keyAllowed(key))
    return fail(error, QStringLiteral("Invalid proxy settings request."));
  if (!protect || !s->value(refKey(key)).toString().isEmpty()) return true;
  const QString value = s->value(key).toString();
  if (hasUserInfo(value)) return save(s, key, value, true, error);
  if (value.contains(QLatin1Char('@')))
    return fail(error, QStringLiteral(
        "A possible credential-bearing proxy URL could not be parsed; "
        "the original setting was left unchanged."));
  return true;
}

} // namespace ProxyCredentials
