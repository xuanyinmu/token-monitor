#include "core/limits/ZcodeDiscovery.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace tmon {

namespace {

QString zcodeDataBaseDir()
{
    const auto fromEnv = qEnvironmentVariable("ZCODE_DATA_BASE_DIR");
    if (!fromEnv.trimmed().isEmpty()) return fromEnv.trimmed();
    const auto windowsInstall = qEnvironmentVariable("ZCODE_WINDOWS_APP_INSTALL_DIR");
    if (!windowsInstall.trimmed().isEmpty()) return windowsInstall.trimmed();
    // USERPROFILE first on Windows: a shell-injected HOME can be an MSYS-style
    // path (/c/…) that Qt's QFile cannot open. Electron reaches the same
    // directory through os.homedir() (USERPROFILE) because HOME is unset in
    // GUI launches.
    const auto userProfile = qEnvironmentVariable("USERPROFILE");
    if (!userProfile.trimmed().isEmpty()) return userProfile.trimmed();
    const auto home = qEnvironmentVariable("HOME");
    if (!home.trimmed().isEmpty()) return home.trimmed();
    return QDir::homePath();
}

QString zcodeDir()
{
    return QDir(zcodeDataBaseDir()).filePath(QStringLiteral(".zcode/v2"));
}

QJsonObject readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QString cleanText(const QString &value)
{
    return value.trimmed();
}

double numberOr(const QJsonValue &value)
{
    return value.isDouble() ? value.toDouble() : 0.0;
}

std::optional<double> numberOrNull(const QJsonValue &value)
{
    if (value.isDouble()) return value.toDouble();
    return std::nullopt;
}

double clampPercent(double value)
{
    if (!std::isfinite(value)) return 0;
    return qBound(0.0, value, 100.0);
}

QString toIso(const QJsonValue &value)
{
    if (value.isString()) {
        const auto raw = value.toString().trimmed();
        if (raw.isEmpty()) return {};
        auto at = QDateTime::fromString(raw, Qt::ISODateWithMs);
        if (!at.isValid()) at = QDateTime::fromString(raw, Qt::ISODate);
        return at.isValid() ? at.toUTC().toString(Qt::ISODateWithMs) : QString();
    }
    if (value.isDouble()) {
        const double ms = value.toDouble();
        if (ms <= 0) return {};
        // Electron toIso: sub-2e10 values are epoch seconds, not millis.
        const qint64 epochMs = ms < 20'000'000'000.0 ? qint64(ms * 1000.0) : qint64(ms);
        return QDateTime::fromMSecsSinceEpoch(epochMs).toUTC().toString(Qt::ISODateWithMs);
    }
    return {};
}

// unit encodings: 5=minutes, 3=hours, 1=days, 6=weeks
qint64 windowMinutes(double unit, double number)
{
    if (number <= 0) return -1;
    const int u = int(unit);
    if (u == 5) return qint64(number);
    if (u == 3) return qint64(number * 60);
    if (u == 1) return qint64(number * 24 * 60);
    if (u == 6) return qint64(number * 7 * 24 * 60);
    return -1;
}

std::optional<double> zaiUsedPercent(const QJsonObject &limit)
{
    const auto total = numberOrNull(limit.value(QStringLiteral("usage")));
    const auto remaining = numberOrNull(limit.value(QStringLiteral("remaining")));
    const auto currentRaw = limit.contains(QStringLiteral("currentValue"))
        ? limit.value(QStringLiteral("currentValue"))
        : limit.value(QStringLiteral("current_value"));
    const auto currentValue = numberOrNull(currentRaw);
    if (total.has_value() && *total > 0) {
        std::optional<double> usedRaw;
        if (remaining.has_value()) {
            const double usedFromRemaining = *total - *remaining;
            usedRaw = currentValue.has_value()
                ? std::optional(std::max(usedFromRemaining, *currentValue))
                : std::optional(usedFromRemaining);
        } else if (currentValue.has_value()) {
            usedRaw = currentValue;
        }
        if (usedRaw.has_value()) {
            const double used = qBound(0.0, *usedRaw, *total);
            return clampPercent(used / *total * 100.0);
        }
    }
    const auto pctRaw = limit.contains(QStringLiteral("percentage"))
        ? limit.value(QStringLiteral("percentage"))
        : (limit.contains(QStringLiteral("usedPercent"))
               ? limit.value(QStringLiteral("usedPercent"))
               : limit.value(QStringLiteral("used_percent")));
    if (pctRaw.isDouble()) return clampPercent(pctRaw.toDouble());
    return std::nullopt;
}

QJsonObject zaiWindow(const QJsonObject &limit, const QString &kind, const QString &label,
                      const QString &fallbackResetAt = {}, bool includeWindowMinutes = true,
                      const QString &resetDescription = {})
{
    const auto usedPercent = zaiUsedPercent(limit);
    if (!usedPercent.has_value()) return {};
    const qint64 minutes = includeWindowMinutes
        ? windowMinutes(numberOr(limit.value(QStringLiteral("unit"))), numberOr(limit.value(QStringLiteral("number"))))
        : qint64(-1);
    auto resetsAt = toIso(limit.contains(QStringLiteral("nextResetTime"))
        ? limit.value(QStringLiteral("nextResetTime"))
        : limit.value(QStringLiteral("next_reset_time")));
    if (resetsAt.isEmpty()) resetsAt = fallbackResetAt;
    QJsonObject window{
        {QStringLiteral("kind"), kind},
        {QStringLiteral("label"), label},
        {QStringLiteral("usedPercent"), *usedPercent},
        {QStringLiteral("remainingPercent"), clampPercent(100.0 - *usedPercent)},
        {QStringLiteral("showMeter"), true}
    };
    if (includeWindowMinutes && minutes >= 0) window.insert(QStringLiteral("windowMinutes"), double(minutes));
    if (!resetsAt.isEmpty()) window.insert(QStringLiteral("resetsAt"), resetsAt);
    if (!resetDescription.isEmpty()) window.insert(QStringLiteral("resetDescription"), resetDescription);
    return window;
}

QString firstTextField(const QJsonObject &source, const QStringList &fields, bool display = false)
{
    for (const auto &field : fields) {
        auto value = cleanText(source.value(field).toString());
        if (!value.isEmpty()) {
            if (display) {
                value.replace(QRegularExpression("[_-]+"), QStringLiteral(" "));
                value.replace(QRegularExpression("\\s+"), QStringLiteral(" "));
                value.replace(QRegularExpression("\\bglm\\b", QRegularExpression::CaseInsensitiveOption),
                              QStringLiteral("GLM"));
                value.replace(QRegularExpression("\\bz\\.?ai\\b", QRegularExpression::CaseInsensitiveOption),
                              QStringLiteral("Z.ai"));
                const auto words = value.split(QLatin1Char(' '));
                QStringList cased;
                for (const auto &word : words)
                    cased.append(word.length() > 1 ? word.at(0).toUpper() + word.mid(1) : word.toUpper());
                value = cased.join(QLatin1Char(' '));
                value.replace(QStringLiteral("Z.Ai"), QStringLiteral("Z.ai"));
                value.replace(QStringLiteral("Z.AI"), QStringLiteral("Z.ai"));
            }
            return value;
        }
    }
    return {};
}

QJsonObject firstSubscription(const QJsonObject &subscriptionBody)
{
    const auto data = subscriptionBody.value(QStringLiteral("data"));
    if (data.isArray()) {
        const auto list = data.toArray();
        return list.isEmpty() ? QJsonObject{} : list.first().toObject();
    }
    if (data.isObject()) {
        const auto list = data.toObject().value(QStringLiteral("list")).toArray();
        if (!list.isEmpty()) return list.first().toObject();
        return data.toObject();
    }
    if (subscriptionBody.value(QStringLiteral("subscriptions")).isArray()) {
        const auto list = subscriptionBody.value(QStringLiteral("subscriptions")).toArray();
        if (!list.isEmpty()) return list.first().toObject();
    }
    return {};
}

QString planFromResponses(const QJsonObject &quotaBody, const QJsonObject &subscriptionBody)
{
    const auto sub = firstSubscription(subscriptionBody);
    const auto subscriptionPlan = firstTextField(sub, {
        QStringLiteral("product_name"), QStringLiteral("productName"),
        QStringLiteral("plan_name"), QStringLiteral("planName"),
        QStringLiteral("package_name"), QStringLiteral("packageName"),
        QStringLiteral("plan"), QStringLiteral("plan_type"), QStringLiteral("planType"),
        QStringLiteral("level")}, true);
    if (!subscriptionPlan.isEmpty()) return subscriptionPlan;
    return firstTextField(quotaBody.value(QStringLiteral("data")).toObject(), {
        QStringLiteral("planName"), QStringLiteral("plan_name"),
        QStringLiteral("packageName"), QStringLiteral("package_name"),
        QStringLiteral("plan"), QStringLiteral("plan_type"), QStringLiteral("planType"),
        QStringLiteral("level")}, true);
}

QString subscriptionResetAt(const QJsonObject &subscriptionBody)
{
    const auto sub = firstSubscription(subscriptionBody);
    const auto raw = sub.contains(QStringLiteral("next_renew_time"))
        ? sub.value(QStringLiteral("next_renew_time"))
        : sub.value(QStringLiteral("nextRenewTime"));
    return toIso(raw);
}

QString entKeyOf(const QJsonObject &balanceOrPlan, const QString &idField)
{
    return QStringLiteral("%1\x1F%2")
        .arg(cleanText(balanceOrPlan.value(QStringLiteral("plan_id")).toString()),
             cleanText(balanceOrPlan.value(idField).toString()));
}

QString hashSeed(const QString &value)
{
    return QString::fromUtf8(QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toHex().left(16));
}

QJsonObject zcodePlanBucketWindow(const QJsonObject &balance, const QHash<QString, QString> &periodByEntitlement)
{
    const auto total = numberOrNull(balance.value(QStringLiteral("total_units")));
    auto used = numberOrNull(balance.value(QStringLiteral("used_units")));
    auto remaining = numberOrNull(balance.value(QStringLiteral("remaining_units")));
    if (!used.has_value() && total.has_value() && remaining.has_value())
        used = std::max(0.0, *total - *remaining);
    if (!remaining.has_value() && total.has_value() && used.has_value())
        remaining = std::max(0.0, *total - *used);
    if (!total.has_value() && !used.has_value() && !remaining.has_value()) return {};
    std::optional<double> usedPercent;
    if (total.has_value() && *total > 0) {
        if (remaining.has_value()) usedPercent = clampPercent(100.0 - (*remaining / *total) * 100.0);
        else if (used.has_value()) usedPercent = clampPercent(*used / *total * 100.0);
    }
    if (!usedPercent.has_value() && balance.value(QStringLiteral("percentage")).isDouble())
        usedPercent = clampPercent(balance.value(QStringLiteral("percentage")).toDouble());
    const auto period = periodByEntitlement.contains(entKeyOf(balance, QStringLiteral("entitlement_id")))
        ? periodByEntitlement.value(entKeyOf(balance, QStringLiteral("entitlement_id")))
        : cleanText(balance.value(QStringLiteral("period")).toString());
    const auto label = cleanText(balance.value(QStringLiteral("show_name")).toString());
    QJsonObject window{
        {QStringLiteral("kind"), period == QLatin1String("daily") ? QStringLiteral("daily") : QStringLiteral("billing")},
        {QStringLiteral("label"), label.isEmpty() ? QStringLiteral("Start Plan") : label},
        {QStringLiteral("limitId"), cleanText(balance.value(QStringLiteral("plan_id")).toString())}
    };
    if (usedPercent.has_value()) {
        window.insert(QStringLiteral("usedPercent"), *usedPercent);
        window.insert(QStringLiteral("remainingPercent"), clampPercent(100.0 - *usedPercent));
    }
    window.insert(QStringLiteral("showMeter"), usedPercent.has_value());
    if (period == QLatin1String("daily")) window.insert(QStringLiteral("windowMinutes"), 24.0 * 60.0);
    if (used.has_value()) window.insert(QStringLiteral("used"), *used);
    if (remaining.has_value()) window.insert(QStringLiteral("remaining"), *remaining);
    if (total.has_value()) window.insert(QStringLiteral("limit"), *total);
    const auto resetsAt = toIso(balance.contains(QStringLiteral("expires_at"))
        ? balance.value(QStringLiteral("expires_at"))
        : balance.value(QStringLiteral("period_end")));
    if (!resetsAt.isEmpty()) {
        window.insert(QStringLiteral("resetsAt"), resetsAt);
        if (period == QLatin1String("daily")) window.insert(QStringLiteral("boundaryKind"), QStringLiteral("reset"));
        else if (period == QLatin1String("one_time")) window.insert(QStringLiteral("boundaryKind"), QStringLiteral("expiry"));
    }
    return window;
}

QHash<QString, QString> zcodePeriodByEntitlement(const QJsonObject &payload)
{
    QHash<QString, QString> out;
    const auto plans = payload.value(QStringLiteral("data")).toObject().value(QStringLiteral("plans")).toArray();
    for (const auto &planV : plans) {
        const auto plan = planV.toObject();
        const auto planId = cleanText(plan.value(QStringLiteral("plan_id")).toString());
        const auto entitlements = plan.value(QStringLiteral("entitlements")).toArray();
        for (const auto &entV : entitlements) {
            const auto entitlement = entV.toObject();
            const auto id = cleanText(entitlement.value(QStringLiteral("entitlement_id")).toString());
            const auto period = cleanText(entitlement.value(QStringLiteral("period")).toString());
            if (!id.isEmpty() && !period.isEmpty())
                out.insert(QStringLiteral("%1\x1F%2").arg(planId, id), period);
        }
    }
    return out;
}

} // namespace

QJsonObject parseZaiUsage(const QJsonObject &quotaBody, const QJsonObject &subscriptionBody)
{
    const auto plan = planFromResponses(quotaBody, subscriptionBody);
    const auto resetAt = subscriptionResetAt(subscriptionBody);
    const auto limits = quotaBody.value(QStringLiteral("data")).toObject().value(QStringLiteral("limits")).toArray();
    QJsonArray windows;
    QList<QJsonObject> tokenLimits;
    QJsonObject timeLimit;
    for (const auto &limitV : limits) {
        const auto limit = limitV.toObject();
        const auto type = cleanText(limit.contains(QStringLiteral("type"))
            ? limit.value(QStringLiteral("type")).toString()
            : limit.value(QStringLiteral("limit_type")).toString()).toUpper();
        if ((type == QLatin1String("TOKENS_LIMIT") || type == QLatin1String("CREDIT_LIMIT"))
            && zaiUsedPercent(limit).has_value()) {
            tokenLimits.append(limit);
        } else if (type == QLatin1String("TIME_LIMIT") && zaiUsedPercent(limit).has_value()) {
            timeLimit = limit;
        }
    }
    std::sort(tokenLimits.begin(), tokenLimits.end(), [](const QJsonObject &a, const QJsonObject &b) {
        const qint64 aMinutes = windowMinutes(numberOr(a.value(QStringLiteral("unit"))), numberOr(a.value(QStringLiteral("number"))));
        const qint64 bMinutes = windowMinutes(numberOr(b.value(QStringLiteral("unit"))), numberOr(b.value(QStringLiteral("number"))));
        const qint64 aKey = aMinutes >= 0 ? aMinutes : std::numeric_limits<qint64>::max();
        const qint64 bKey = bMinutes >= 0 ? bMinutes : std::numeric_limits<qint64>::max();
        return aKey < bKey;
    });
    const bool singleIsSession = tokenLimits.size() == 1
        && [&] {
            const qint64 minutes = windowMinutes(numberOr(tokenLimits.first().value(QStringLiteral("unit"))),
                                                 numberOr(tokenLimits.first().value(QStringLiteral("number"))));
            return minutes >= 0 && minutes <= 6 * 60;
        }();
    const bool hasSessionLimit = tokenLimits.size() >= 2 || singleIsSession;
    const QJsonObject sessionTokenLimit = hasSessionLimit ? tokenLimits.first() : QJsonObject{};
    const QJsonObject tokenLimit = tokenLimits.size() >= 2
        ? tokenLimits.last()
        : (hasSessionLimit ? QJsonObject{}
                           : (tokenLimits.isEmpty() ? QJsonObject{} : tokenLimits.first()));

    const auto fiveHour = sessionTokenLimit.isEmpty()
        ? QJsonObject{}
        : zaiWindow(sessionTokenLimit, QStringLiteral("session"), QStringLiteral("5-hour"));
    if (!fiveHour.isEmpty()) windows.append(fiveHour);

    const auto weekly = tokenLimit.isEmpty()
        ? QJsonObject{}
        : zaiWindow(tokenLimit, QStringLiteral("weekly"), QStringLiteral("Weekly"));
    if (!weekly.isEmpty()) windows.append(weekly);

    // The MCP TIME_LIMIT carries a misleading unit=5/number=1 window marker;
    // drop windowMinutes and carry a Monthly cadence instead.
    auto mcp = timeLimit.isEmpty()
        ? QJsonObject{}
        : zaiWindow(timeLimit, QStringLiteral("billing"), QStringLiteral("MCP"), resetAt, false, QStringLiteral("Monthly"));
    if (!mcp.isEmpty()) {
        const auto remaining = numberOrNull(timeLimit.value(QStringLiteral("remaining")));
        if (remaining.has_value()) mcp.insert(QStringLiteral("remaining"), *remaining);
        windows.append(mcp);
    }
    return QJsonObject{
        {QStringLiteral("plan"), plan},
        {QStringLiteral("windows"), windows}
    };
}

QJsonObject parseZcodeStartPlanBalances(const QJsonObject &payload)
{
    const auto periodByEntitlement = zcodePeriodByEntitlement(payload);
    const auto balances = payload.value(QStringLiteral("data")).toObject().value(QStringLiteral("balances")).toArray();
    struct Group {
        QStringList periods;
        QList<QJsonObject> windows;
    };
    QHash<QString, Group> groups;
    QList<QString> order;
    for (int index = 0; index < balances.size(); ++index) {
        const auto balance = balances[index].toObject();
        const auto window = zcodePlanBucketWindow(balance, periodByEntitlement);
        if (window.isEmpty()) continue;
        const auto identity = cleanText(balance.value(QStringLiteral("show_name")).toString()).toLower();
        const auto period = periodByEntitlement.contains(entKeyOf(balance, QStringLiteral("entitlement_id")))
            ? periodByEntitlement.value(entKeyOf(balance, QStringLiteral("entitlement_id")))
            : cleanText(balance.value(QStringLiteral("period")).toString());
        const bool complete = window.value(QStringLiteral("limit")).isDouble()
            && window.value(QStringLiteral("limit")).toDouble() > 0
            && window.value(QStringLiteral("remaining")).isDouble();
        // Unknown identity or incomplete numbers cannot safely be added.
        const QString key = identity.isEmpty() || !complete
            ? QStringLiteral("%1\x1F%2").arg(identity, QString::number(index))
            : identity;
        if (!groups.contains(key)) {
            order.append(key);
            groups.insert(key, Group{});
        }
        groups[key].periods.append(period);
        groups[key].windows.append(window);
    }
    QJsonArray windows;
    for (const auto &key : order) {
        const auto group = groups.value(key);
        auto entries = group.windows;
        std::sort(entries.begin(), entries.end(), [](const QJsonObject &a, const QJsonObject &b) {
            const auto aLabel = a.value(QStringLiteral("label")).toString();
            const auto bLabel = b.value(QStringLiteral("label")).toString();
            if (aLabel != bLabel) return aLabel < bLabel;
            return a.value(QStringLiteral("limitId")).toString() < b.value(QStringLiteral("limitId")).toString();
        });
        auto window = entries.first();
        if (entries.size() > 1) {
            double limit = 0, remaining = 0, used = 0;
            for (const auto &entry : entries) {
                limit += entry.value(QStringLiteral("limit")).toDouble();
                remaining += entry.value(QStringLiteral("remaining")).toDouble();
                used += entry.value(QStringLiteral("used")).toDouble();
            }
            window.insert(QStringLiteral("limit"), limit);
            window.insert(QStringLiteral("remaining"), remaining);
            window.insert(QStringLiteral("used"), used);
            const double pct = limit > 0 ? clampPercent(used / limit * 100.0) : 0;
            window.insert(QStringLiteral("usedPercent"), pct);
            window.insert(QStringLiteral("remainingPercent"), clampPercent(100.0 - pct));
            window.insert(QStringLiteral("limitId"), QStringLiteral("zcode-model:") + hashSeed(key));
        } else if (cleanText(window.value(QStringLiteral("limitId")).toString()).isEmpty()) {
            window.insert(QStringLiteral("limitId"), QStringLiteral("zcode-bucket:") + hashSeed(key));
        }
        // Boundary picking: earliest resetsAt wins; kind is kept only when every
        // boundary at that instant carries one.
        QList<QPair<QString, QString>> boundaries;
        for (const auto &entry : entries) {
            const auto at = cleanText(entry.value(QStringLiteral("resetsAt")).toString());
            if (at.isEmpty()) continue;
            boundaries.append({at, cleanText(entry.value(QStringLiteral("boundaryKind")).toString())});
        }
        std::sort(boundaries.begin(), boundaries.end());
        if (!boundaries.isEmpty()) {
            const auto nextAt = boundaries.first().first;
            QSet<QString> nextKinds;
            bool allKinds = true;
            for (const auto &boundary : boundaries) {
                if (boundary.first != nextAt) continue;
                if (boundary.second.isEmpty()) allKinds = false;
                else nextKinds.insert(boundary.second);
            }
            window.insert(QStringLiteral("resetsAt"), nextAt);
            if (allKinds) {
                window.insert(QStringLiteral("boundaryKind"),
                              nextKinds.size() > 1 ? QStringLiteral("mixed") : *nextKinds.begin());
            } else {
                window.remove(QStringLiteral("boundaryKind"));
            }
        }
        // Non-uniform daily buckets collapse onto the shared billing lane.
        QSet<QString> periods(group.periods.cbegin(), group.periods.cend());
        bool uniformDaily = periods.size() == 1 && group.periods.value(0) == QLatin1String("daily")
            && !boundaries.isEmpty();
        if (uniformDaily) {
            for (const auto &boundary : boundaries)
                if (boundary.first != boundaries.first().first) uniformDaily = false;
        }
        if (!uniformDaily) {
            window.insert(QStringLiteral("kind"), QStringLiteral("billing"));
            window.remove(QStringLiteral("windowMinutes"));
        }
        windows.append(window);
    }
    // Prefer renewing daily entitlements, then a stable identity as tie-break.
    QList<QJsonObject> activePlans;
    const auto plans = payload.value(QStringLiteral("data")).toObject().value(QStringLiteral("plans")).toArray();
    for (const auto &planV : plans) {
        const auto plan = planV.toObject();
        if (cleanText(plan.value(QStringLiteral("status")).toString()) != QLatin1String("active")) continue;
        activePlans.append(plan);
    }
    std::sort(activePlans.begin(), activePlans.end(), [](const QJsonObject &a, const QJsonObject &b) {
        const auto hasDaily = [](const QJsonObject &plan) {
            const auto entitlements = plan.value(QStringLiteral("entitlements")).toArray();
            for (const auto &entV : entitlements)
                if (cleanText(entV.toObject().value(QStringLiteral("period")).toString()) == QLatin1String("daily"))
                    return true;
            return false;
        };
        const int aDaily = hasDaily(a) ? 1 : 0;
        const int bDaily = hasDaily(b) ? 1 : 0;
        if (aDaily != bDaily) return aDaily > bDaily;
        const auto idOf = [](const QJsonObject &plan) {
            return cleanText(plan.contains(QStringLiteral("plan_id"))
                ? plan.value(QStringLiteral("plan_id")).toString()
                : plan.value(QStringLiteral("name")).toString());
        };
        return idOf(a) < idOf(b);
    });
    const auto plan = activePlans.isEmpty() ? QString() : cleanText(activePlans.first().value(QStringLiteral("name")).toString());
    return QJsonObject{
        {QStringLiteral("plan"), plan},
        {QStringLiteral("windows"), windows}
    };
}

ZcodeConnection discoverZcodeConnection()
{
    const auto base = zcodeDir();
    const auto settings = readJson(QDir(base).filePath(QStringLiteral("setting.json")));
    const auto registry = readJson(QDir(base).filePath(QStringLiteral("config.json")));
    if (settings.isEmpty() || registry.isEmpty()) return {};
    const auto domain = cleanText(settings.value(QStringLiteral("providerFamilyDomain")).toString());
    if (domain != QLatin1String("zai") && domain != QLatin1String("bigmodel")) return {};
    const auto selected = cleanText(settings.value(QStringLiteral("modelProviderFamilySelectedKeys"))
                                    .toObject().value(domain).toString());
    static const QRegularExpression selectedRe(QStringLiteral("^(?:coding-plan|preset):(.+)$"));
    const auto match = selectedRe.match(selected);
    const auto providerId = match.hasMatch() ? match.captured(1).trimmed() : QString();
    if (providerId.isEmpty()) return {};
    const auto provider = registry.value(QStringLiteral("provider")).toObject().value(providerId).toObject();
    // A disabled entry means a family switch has not settled yet; skip this round.
    if (provider.isEmpty()) return {};
    const auto enabledVal = provider.value(QStringLiteral("enabled"));
    if (enabledVal.isBool() && !enabledVal.toBool()) return {};

    const bool startPlan = providerId == QLatin1String("builtin:zai-start-plan")
        || providerId == QLatin1String("builtin:bigmodel-start-plan");
    const bool codingPlan = providerId == QLatin1String("builtin:zai-coding-plan")
        || providerId == QLatin1String("builtin:bigmodel-coding-plan");
    ZcodeConnection out;
    out.family = domain;
    out.providerId = providerId;
    out.deviceMid = cleanText(readJson(QDir(base).filePath(QStringLiteral("telemetry-state.json")))
                              .value(QStringLiteral("deviceMid")).toString());
    if (startPlan || codingPlan) {
        const auto cache = readJson(QDir(base).filePath(QStringLiteral("coding-plan-cache.json")));
        const auto entry = cache.value(QStringLiteral("entryStatus")).toObject()
                               .value(QStringLiteral("items")).toObject().value(providerId).toObject();
        out.entitled = cleanText(entry.value(QStringLiteral("status")).toString()) == QLatin1String("available");
        out.reason = out.entitled
            ? QString()
            : (cleanText(entry.value(QStringLiteral("reason")).toString()).isEmpty()
                   ? QStringLiteral("coding_plan_not_entitled")
                   : cleanText(entry.value(QStringLiteral("reason")).toString()));
        out.kind = startPlan ? QStringLiteral("start-billing") : QStringLiteral("coding-quota");
        // The mirror key inside the provider entry is the only readable token.
        const auto mirrorKey = cleanText(provider.value(QStringLiteral("options")).toObject()
                                         .value(QStringLiteral("apiKey")).toString());
        if (out.entitled && mirrorKey.isEmpty()) {
            out.entitled = false;
            out.reason = QStringLiteral("coding_plan_not_authenticated");
            return out;
        }
        out.mirrorKey = mirrorKey;
        if (out.kind == QLatin1String("coding-quota")) {
            // Billing is account-level: ZCode queries it with the start-plan
            // entry even while coding-plan is selected.
            const auto startProviderId = domain == QLatin1String("bigmodel")
                ? QStringLiteral("builtin:bigmodel-start-plan")
                : QStringLiteral("builtin:zai-start-plan");
            const auto startEntry = cache.value(QStringLiteral("entryStatus")).toObject()
                                        .value(QStringLiteral("items")).toObject().value(startProviderId).toObject();
            const auto startProvider = registry.value(QStringLiteral("provider")).toObject()
                                           .value(startProviderId).toObject();
            if (cleanText(startEntry.value(QStringLiteral("status")).toString()) == QLatin1String("available")
                && !startProvider.isEmpty()) {
                const auto startKey = cleanText(startProvider.value(QStringLiteral("options")).toObject()
                                                .value(QStringLiteral("apiKey")).toString());
                if (!startKey.isEmpty()) out.billingKey = startKey;
            }
        }
        return out;
    }
    out.kind = QStringLiteral("api-unsupported");
    out.entitled = false;
    out.reason = QStringLiteral("api_balance_not_supported");
    return out;
}

} // namespace tmon
