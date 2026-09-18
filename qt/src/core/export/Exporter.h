#pragma once

#include <QJsonObject>
#include <QString>

namespace tmon {

bool exportNow(const QJsonObject &stats, const QString &dir);
bool exportDiagnostics(const QJsonObject &redactedSettings, const QJsonObject &stats,
                       const QString &lastError, const QString &dir);

} // namespace tmon
