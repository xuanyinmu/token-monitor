#include "core/limits/LimitsCore.h"

#include "core/catalog/Catalog.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSet>
#include <QStringList>

namespace tmon {
namespace {

const QStringList kWindowOrder{QStringLiteral("session"), QStringLiteral("daily"), QStringLiteral("weekly"), QStringLiteral("billing")};
const QSet<QString> kStatuses{
    QStringLiteral("ok"), QStringLiteral("disabled"), QStringLiteral("notConfigured"), QStringLiteral("unauthorized"),
    QStringLiteral("rateLimited"), QStringLiteral("sourceRateLimited"), QStringLiteral("unavailable"), QStringLiteral("error")
};
const QSet<QString> kSources{
    QStringLiteral("oauth"), QStringLiteral("cli"), QStringLiteral("web"), QStringLiteral("rpc"),
    QStringLiteral("local"), QStringLiteral("api")
};

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

} // namespace

QJsonObject normalizeLimitWindow(const QJsonObject &input)
{
    QString kind = input.value(QStringLiteral("kind")).toString().toLower();
    kind.replace(QLatin1Char('_'), QString());
    kind.replace(QLatin1Char('-'), QString());
    kind.replace(QLatin1Char(' '), QString());
    if (kind == QLatin1String("billingcycle") || kind == QLatin1String("monthly")) kind = QStringLiteral("billing");
    if (!kWindowOrder.contains(kind)) kind = QStringLiteral("billing");
    const auto usedPercentVal = input.value(QStringLiteral("usedPercent"));
    QJsonValue remainingPercentVal = input.value(QStringLiteral("remainingPercent"));
    if (remainingPercentVal.isUndefined() || remainingPercentVal.isNull()) {
        if (usedPercentVal.isDouble())
            remainingPercentVal = 100.0 - usedPercentVal.toDouble();
    }
    QJsonObject out{
        {QStringLiteral("kind"), kind},
        {QStringLiteral("label"), input.value(QStringLiteral("label")).toString().left(32)},
        {QStringLiteral("used"), input.value(QStringLiteral("used"))},
        {QStringLiteral("limit"), input.value(QStringLiteral("limit"))},
        {QStringLiteral("remaining"), input.value(QStringLiteral("remaining"))},
        {QStringLiteral("usedPercent"), usedPercentVal},
        {QStringLiteral("remainingPercent"), remainingPercentVal},
        {QStringLiteral("resetsAt"), input.value(QStringLiteral("resetsAt"))},
        {QStringLiteral("showMeter"), input.value(QStringLiteral("showMeter")).toBool(true)}
    };
    const auto metric = input.value(QStringLiteral("metric")).toString();
    if (metric == QLatin1String("credits") || metric == QLatin1String("spend"))
        out.insert(QStringLiteral("metric"), metric);
    if (input.contains(QStringLiteral("currency")))
        out.insert(QStringLiteral("currency"), input.value(QStringLiteral("currency")).toString().toUpper().left(8));
    return out;
}

QJsonObject normalizeLimitProvider(const QJsonObject &input)
{
    const auto id = input.value(QStringLiteral("provider")).toString();
    if (!limitProviderIds().contains(id)) return {};
    QJsonArray windows;
    for (const auto &w : input.value(QStringLiteral("windows")).toArray()) {
        const auto n = normalizeLimitWindow(w.toObject());
        if (!n.isEmpty()) windows.append(n);
    }
    auto status = input.value(QStringLiteral("status")).toString(QStringLiteral("error"));
    if (!kStatuses.contains(status)) status = QStringLiteral("error");
    auto source = input.value(QStringLiteral("source")).toString();
    if (!kSources.contains(source)) source.clear();
    QJsonObject out{
        {QStringLiteral("provider"), id},
        {QStringLiteral("accountKey"), input.value(QStringLiteral("accountKey")).toString()},
        {QStringLiteral("accountLabel"), input.value(QStringLiteral("accountLabel")).toString()},
        {QStringLiteral("planLabel"), input.value(QStringLiteral("planLabel")).toString()},
        {QStringLiteral("accountEmail"), input.value(QStringLiteral("accountEmail")).toString()},
        {QStringLiteral("status"), status},
        {QStringLiteral("source"), source},
        {QStringLiteral("updatedAt"), input.value(QStringLiteral("updatedAt")).toString(nowIso())},
        {QStringLiteral("windows"), windows}
    };
    if (input.contains(QStringLiteral("balance"))) out.insert(QStringLiteral("balance"), input.value(QStringLiteral("balance")));
    if (input.contains(QStringLiteral("region"))) out.insert(QStringLiteral("region"), input.value(QStringLiteral("region")));
    return out;
}

QJsonObject notConfiguredProvider(const QString &id, const QString &source)
{
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), id},
        {QStringLiteral("source"), source},
        {QStringLiteral("status"), QStringLiteral("notConfigured")},
        {QStringLiteral("windows"), QJsonArray{}}
    });
}

QJsonObject errorProvider(const QString &id, const QString &source, const QString &status, const QString &accountKey)
{
    return normalizeLimitProvider(QJsonObject{
        {QStringLiteral("provider"), id},
        {QStringLiteral("source"), source},
        {QStringLiteral("status"), status},
        {QStringLiteral("accountKey"), accountKey},
        {QStringLiteral("windows"), QJsonArray{}}
    });
}

} // namespace tmon
