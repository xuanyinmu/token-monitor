#pragma once

#include <QString>
#include <QStringList>

namespace tmon {

QString hashKey(const QStringList &parts);
QString hashKey(const QString &a, const QString &b = {}, const QString &c = {});

} // namespace tmon
