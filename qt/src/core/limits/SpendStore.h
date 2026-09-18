#pragma once

#include <QJsonObject>
#include <QString>

namespace tmon {

class SpendStore {
public:
    QJsonObject record(const QString &provider, const QString &accountKey, const QString &currency,
                       double paid, double remaining);
};

} // namespace tmon
