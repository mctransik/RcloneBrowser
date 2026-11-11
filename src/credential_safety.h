#pragma once

#include <QString>

namespace CredentialSafety {

// Checks only whether rclone requested the encrypted configuration password.
// Do not treat this as a general-purpose secret detector or log redactor.
inline bool requiresConfigPassword(const QString &errorOutput) {
  return errorOutput.contains(QStringLiteral("RCLONE_CONFIG_PASS"));
}

} // namespace CredentialSafety
