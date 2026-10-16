#pragma once
#include <QString>

namespace TaskSecretRisk {
// Best-effort warning for known patterns, not encryption or proof of safety.
// The detected secret and matching text must never be displayed.
bool mayContainCredentials(const QString &extra, const QString &source,
                           const QString &destination);
} // namespace TaskSecretRisk
