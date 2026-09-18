#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace tmon {

class HttpClient;
class SpendStore;

QJsonArray fetchAllLimitProviders(const QJsonObject &settings, HttpClient &http, SpendStore &spend);
QJsonObject fetchLimitProvider(const QString &id, const QJsonObject &settings, HttpClient &http, SpendStore &spend);

} // namespace tmon
