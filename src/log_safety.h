#pragma once
#include <QString>
namespace LogSafety {
// Best-effort masking of common secrets in subprocess output.
// Do NOT treat masked logs as safe for external sharing.
QString maskKnownCredentials(const QString &text);
} // namespace LogSafety
