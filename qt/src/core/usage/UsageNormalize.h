#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace tmon {

QJsonValue applyTokscaleSessionMetadata(QJsonValue json, bool resolveProjects = true);
QJsonObject projectIdentity(const QString &path);
QJsonObject extractUsageFromTokscale(const QJsonValue &json, bool resolveProjects = true);
QJsonObject extractUsageBundleFromTokscale(const QJsonValue &json);
QJsonObject normalizeDeviceRecord(const QJsonObject &record);
QJsonObject mergeDeviceRecord(const QJsonObject &existing, const QJsonObject &incoming);
QJsonObject aggregateDevices(const QJsonArray &devices, int staleAfterMs, qint64 nowMs = 0);
QJsonObject stripSessionTextFromDeviceRecord(const QJsonObject &record);
QJsonObject periodWindowsNow();
bool isPeriodExpired(const QJsonObject &record, const QString &periodName, qint64 nowMs);

} // namespace tmon
