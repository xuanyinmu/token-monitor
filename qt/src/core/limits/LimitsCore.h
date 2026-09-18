#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

namespace tmon {

QJsonObject normalizeLimitWindow(const QJsonObject &input);
QJsonObject normalizeLimitProvider(const QJsonObject &input);
QJsonObject notConfiguredProvider(const QString &id, const QString &source = QStringLiteral("api"));
QJsonObject errorProvider(const QString &id, const QString &source, const QString &status, const QString &accountKey = {});

} // namespace tmon
