#pragma once

#include <QByteArray>
#include <QString>

// Standalone envelope codec. Deliberately NOT yet integrated with tasks.bin:
// legacy migration, portable recovery and load-failure behavior must be
// designed and tested before any existing user file is rewritten.
namespace TaskProtection {

enum class Format { LegacyPlaintext, WindowsDpapiV1, UnsupportedProtected };

// Versioned fixed header identifies protected data without guessing based on
// QDataStream content. Unknown future protected versions must fail closed.
Format inspectFormat(const QByteArray &stored);

// Windows DPAPI, scoped to the current Windows user (not machine-wide).
// Returns false on portable/non-Windows hosts; never writes unencrypted output
// as a fallback. Error strings are generic and contain no private file data.
bool encryptForWindowsUser(const QByteArray &plaintext, QByteArray *stored,
                           QString *error = nullptr);
bool decryptForWindowsUser(const QByteArray &stored, QByteArray *plaintext,
                           QString *error = nullptr);

} // namespace TaskProtection
