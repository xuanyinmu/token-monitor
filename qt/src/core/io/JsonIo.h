#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace tmon {

QJsonDocument readJsonDocument(const QString &path, bool *ok = nullptr);
QJsonObject readJsonObject(const QString &path);
QJsonArray readJsonArray(const QString &path);
bool writeJsonAtomic(const QString &path, const QJsonValue &value);
bool writePrivateJsonAtomic(const QString &path, const QJsonValue &value);
QString jsonString(const QJsonValue &value);
QJsonValue jsonClone(const QJsonValue &value);

} // namespace tmon
