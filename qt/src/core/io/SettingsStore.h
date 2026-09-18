#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

namespace tmon {

class SettingsStore {
public:
    static QJsonObject defaults();
    static QJsonObject load();
    static bool save(const QJsonObject &settings);
    static QJsonObject merge(const QJsonObject &stored);
    static QVariantMap toVariant(const QJsonObject &settings);
};

} // namespace tmon
