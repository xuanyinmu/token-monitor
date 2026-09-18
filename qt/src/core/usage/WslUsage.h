#pragma once

#include <QJsonObject>
#include <QStringList>

namespace tmon {

QJsonObject scanWslUsage(const QStringList &clients, const QString &allTimeSince = QStringLiteral("2024-01-01"));

} // namespace tmon
