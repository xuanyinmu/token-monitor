#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace tmon {

struct ClientInfo {
    QString id;
    QString label;
    bool defaultTracked = true;
    bool locallyParsed = false;
};

struct LimitProviderInfo {
    QString id;
    QString label;
    QString settingsLabel;
};

QVector<ClientInfo> clientCatalog();
QStringList clientIds();
QStringList defaultClientIds();
QStringList locallyParsedClientIds();
QString defaultClientsCsv();
QString knownClientsCsv();
QString clientLabel(const QString &id);
QString normalizeTrackedClientId(const QString &value);
QString normalizeClientsCsv(const QString &value);
QString clientsCsvForSetting(const QString &value);

QVector<LimitProviderInfo> limitProviderCatalog();
QStringList limitProviderIds();
QString defaultLimitProvidersCsv();
QString limitProviderLabel(const QString &id);
QString limitProviderSettingsLabel(const QString &id);
QString limitProviderForClient(const QString &clientId);
QStringList tokscaleScanClientIds(const QString &clientId);

} // namespace tmon
