#pragma once

#include <QJsonValue>
#include <QString>

namespace tmon {

QJsonValue applyPeriodDelta(const QJsonValue &base, const QJsonValue &freshToday, const QJsonValue &anchorToday);
QJsonValue deltaValue(const QJsonValue &base, const QJsonValue &fresh, const QJsonValue &anchor, const QString &key);

} // namespace tmon
