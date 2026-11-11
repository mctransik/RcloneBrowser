#pragma once

#include <QSettings>
#include <QString>

namespace ProxyCredentials {
// Only Settings/http_proxy and Settings/https_proxy are accepted.
bool hasUserInfo(const QString &url);
QString withoutUserInfo(const QString &url);
bool load(QSettings *settings, const QString &key, QString *value,
          QString *error = nullptr);
bool save(QSettings *settings, const QString &key, const QString &value,
          bool protect, QString *error = nullptr);
bool migrateLegacy(QSettings *settings, const QString &key, bool protect,
                   QString *error = nullptr);
}
