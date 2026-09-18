#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVariantMap>

namespace tmon {

class CredentialStore {
public:
    static QJsonObject load();
    static bool save(const QJsonObject &document);
    static QString get(const QString &settingsKey);
    static QJsonValue getValue(const QString &settingsKey);
    static void set(const QString &settingsKey, const QJsonValue &value);
    static QJsonObject overlayOnto(const QJsonObject &settings);
    static QVariantMap redactedForUi(const QJsonObject &settings);
};

} // namespace tmon
