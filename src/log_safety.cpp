#include "log_safety.h"
#include <QRegularExpression>

namespace LogSafety {
QString maskKnownCredentials(const QString &text) {
  QString output = text;
  // URL userinfo: leave host/path intact but hide everything before '@'.
  static const QRegularExpression urlUserInfo(
      QStringLiteral(R"rx(\b((?:https?|socks5)://)[^\s/?#<>"']+@)rx"),
      QRegularExpression::CaseInsensitiveOption);
  output.replace(urlUserInfo, QStringLiteral("\\1[REDACTED]@"));

  // Known token-bearing URL query keys. Vendor-specific names remain unhandled.
  static const QRegularExpression queryCredentials(
      QStringLiteral(R"rx(([?&](?:access_token|refresh_token|id_token|api_key|apikey|client_secret|password|token)=)[^&#\s<>"']+)rx"),
      QRegularExpression::CaseInsensitiveOption);
  output.replace(queryCredentials, QStringLiteral("\\1[REDACTED]"));

  static const QRegularExpression authorization(
      QStringLiteral(R"rx(\b((?:Proxy-)?Authorization\s*:\s*(?:Bearer|Basic)\s+)[A-Za-z0-9._~+/\-=]+)rx"),
      QRegularExpression::CaseInsensitiveOption);
  output.replace(authorization, QStringLiteral("\\1[REDACTED]"));

  static const QRegularExpression assignments(
      QStringLiteral(R"rx(\b((?:RCLONE_CONFIG_PASS|AWS_SECRET_ACCESS_KEY|RCLONE_[A-Z0-9_]+_TOKEN|access_token|refresh_token|client_secret|api_key|password|token)\s*=\s*)(?:"[^"]*"|'[^']*'|[^\s,;]+))rx"),
      QRegularExpression::CaseInsensitiveOption);
  output.replace(assignments, QStringLiteral("\\1[REDACTED]"));

  static const QRegularExpression flags(
      QStringLiteral(R"rx((--(?:password|token|access-token|refresh-token|client-secret|api-key)(?:=|\s+))(?:"[^"]*"|'[^']*'|[^\s]+))rx"),
      QRegularExpression::CaseInsensitiveOption);
  output.replace(flags, QStringLiteral("\\1[REDACTED]"));
  return output;
}
} // namespace LogSafety
