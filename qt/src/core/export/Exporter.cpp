#include "core/export/Exporter.h"

#include "core/usage/JsonUtil.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTextStream>

namespace tmon {

bool exportNow(const QJsonObject &stats, const QString &dir)
{
    if (dir.isEmpty()) return false;
    QDir().mkpath(dir);
    const auto stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QFile jsonFile(QDir(dir).filePath(QStringLiteral("token-monitor-") + stamp + QStringLiteral(".json")));
    if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    jsonFile.write(QJsonDocument(stats).toJson(QJsonDocument::Indented));
    jsonFile.close();

    QFile csvFile(QDir(dir).filePath(QStringLiteral("token-monitor-") + stamp + QStringLiteral(".csv")));
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QTextStream out(&csvFile);
    out << "period,client,tokens,costUsd\n";
    const auto periods = stats.value(QStringLiteral("periods")).toObject();
    for (const auto &name : kPeriods) {
        const auto period = periods.value(name).toObject();
        const auto clients = period.value(QStringLiteral("clients")).toObject();
        const auto costs = period.value(QStringLiteral("clientCosts")).toObject();
        for (auto it = clients.begin(); it != clients.end(); ++it) {
            out << name << ',' << it.key() << ',' << asNumber(it.value()) << ',' << asNumber(costs.value(it.key())) << '\n';
        }
    }
    return true;
}

bool exportDiagnostics(const QJsonObject &redactedSettings, const QJsonObject &stats,
                       const QString &lastError, const QString &dir)
{
    if (dir.isEmpty()) return false;
    QDir().mkpath(dir);
    const auto stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QFile file(QDir(dir).filePath(QStringLiteral("token-monitor-diagnostics-") + stamp + QStringLiteral(".json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    const auto payload = QJsonObject{
        {QStringLiteral("exportedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("settings"), redactedSettings},
        {QStringLiteral("stats"), stats},
        {QStringLiteral("lastError"), lastError}
    };
    file.write(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    return true;
}

} // namespace tmon
