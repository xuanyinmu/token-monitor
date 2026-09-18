#include "core/limits/SpendStore.h"

#include "core/io/JsonIo.h"
#include "core/io/Paths.h"

#include <QDateTime>
#include <QDir>
#include <QJsonArray>

namespace tmon {

QJsonObject SpendStore::record(const QString &provider, const QString &accountKey,
                               const QString &currency, double paid, double remaining)
{
    const auto path = QDir(Paths::sharedDataDir()).filePath(provider + QStringLiteral("-balance-v2.json"));
    auto store = readJsonObject(path);
    const auto now = QDateTime::currentMSecsSinceEpoch();
    auto accounts = store.value(QStringLiteral("accounts")).toObject();
    auto acc = accounts.value(accountKey).toObject();
    const double prevPaid = acc.value(QStringLiteral("paid")).toDouble();
    double spend = acc.value(QStringLiteral("spend")).toDouble();
    if (prevPaid > 0 && paid < prevPaid) spend += (prevPaid - paid);
    acc.insert(QStringLiteral("paid"), paid);
    acc.insert(QStringLiteral("remaining"), remaining);
    acc.insert(QStringLiteral("currency"), currency);
    acc.insert(QStringLiteral("spend"), spend);
    acc.insert(QStringLiteral("updatedAt"), now);
    accounts.insert(accountKey, acc);
    store.insert(QStringLiteral("accounts"), accounts);
    QDir().mkpath(Paths::sharedDataDir());
    writeJsonAtomic(path, store);
    return QJsonObject{
        {QStringLiteral("metric"), QStringLiteral("spend")},
        {QStringLiteral("kind"), QStringLiteral("billing")},
        {QStringLiteral("label"), QStringLiteral("Month spend")},
        {QStringLiteral("used"), spend},
        {QStringLiteral("currency"), currency},
        {QStringLiteral("showMeter"), false}
    };
}

} // namespace tmon
