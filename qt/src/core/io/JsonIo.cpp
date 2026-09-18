#include "core/io/JsonIo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace tmon {
namespace {

QJsonDocument parseFile(const QString &path, bool *ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (ok) *ok = false;
        return {};
    }
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (ok) *ok = err.error == QJsonParseError::NoError;
    return doc;
}

bool writeAtomic(const QString &path, const QJsonValue &value, bool privateFile)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    QJsonDocument doc;
    if (value.isObject()) doc = QJsonDocument(value.toObject());
    else if (value.isArray()) doc = QJsonDocument(value.toArray());
    else doc = QJsonDocument(QJsonArray{value});
    if (file.write(doc.toJson(QJsonDocument::Indented)) < 0) return false;
    const bool ok = file.commit();
    if (ok && privateFile) {
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    return ok;
}

} // namespace

QJsonDocument readJsonDocument(const QString &path, bool *ok)
{
    return parseFile(path, ok);
}

QJsonObject readJsonObject(const QString &path)
{
    bool ok = false;
    const auto doc = parseFile(path, &ok);
    return ok && doc.isObject() ? doc.object() : QJsonObject{};
}

QJsonArray readJsonArray(const QString &path)
{
    bool ok = false;
    const auto doc = parseFile(path, &ok);
    return ok && doc.isArray() ? doc.array() : QJsonArray{};
}

bool writeJsonAtomic(const QString &path, const QJsonValue &value)
{
    return writeAtomic(path, value, false);
}

bool writePrivateJsonAtomic(const QString &path, const QJsonValue &value)
{
    return writeAtomic(path, value, true);
}

QString jsonString(const QJsonValue &value)
{
    if (value.isString()) return value.toString();
    if (value.isDouble()) return QString::number(value.toDouble());
    if (value.isBool()) return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return {};
}

QJsonValue jsonClone(const QJsonValue &value)
{
    return value;
}

} // namespace tmon
