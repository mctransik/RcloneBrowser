#include "task_secret_risk.h"
#include <QRegularExpression>

namespace TaskSecretRisk {

// Heuristic warning only: this does not detect every credential type, and
// tasks.bin remains unencrypted. Never display the original matching value.
bool mayContainCredentials(const QString &extra,
                                  const QString &source,
                                  const QString &destination) {
  const QString all = extra + QLatin1Char('\n') + source + QLatin1Char('\n') +
                      destination;

  static const QRegularExpression secretFlag(
      QStringLiteral(
          R"rx((?:^|\s)--(?:password|pass|token|access-token|refresh-token|api-key|client-secret|s3-secret-access-key|drive-client-secret|b2-account-key|azureblob-account-key|ftp-pass|sftp-pass|http-pass|rc-pass)(?:=|\s))rx"),
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression credentialUrl(
      QStringLiteral(R"rx(\b[a-z][a-z0-9+.-]*://[^\s/?#<>"']+@)rx"),
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression assignment(
      QStringLiteral(
          R"rx((?:^|[\s?&])(?:access_token|refresh_token|id_token|api_key|apikey|client_secret|password|token|secret_access_key|s3_secret_access_key)\s*[:=])rx"),
      QRegularExpression::CaseInsensitiveOption);
  static const QRegularExpression authHeader(
      QStringLiteral(
          R"rx((?:(?:Proxy-)?Authorization\s*:\s*(?:Bearer|Basic)\b|X-(?:Api-Key|Auth-Token)\s*:))rx"),
      QRegularExpression::CaseInsensitiveOption);

  return secretFlag.match(all).hasMatch() || credentialUrl.match(all).hasMatch() ||
         assignment.match(all).hasMatch() || authHeader.match(all).hasMatch();
}
} // namespace TaskSecretRisk
