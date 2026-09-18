#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

namespace tmon {

QJsonObject mergeLocalParsers(const QJsonObject &period, const QStringList &clients, const QString &window);

} // namespace tmon
