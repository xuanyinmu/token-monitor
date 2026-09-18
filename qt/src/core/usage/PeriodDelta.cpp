#include "core/usage/PeriodDelta.h"

#include "core/usage/JsonUtil.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <algorithm>
#include <cmath>

namespace tmon {

QJsonValue deltaValue(const QJsonValue &base, const QJsonValue &fresh, const QJsonValue &anchor, const QString &key)
{
    if (key == QLatin1String("tokenComponents"))
        return base.toBool() && fresh.toBool();
    if (key == QLatin1String("throughput"))
        return base.toBool() && fresh.toBool() && anchor.toBool();
    if (key == QLatin1String("startedAt")) {
        const auto baseMs = timestampMs(base);
        const auto freshMs = timestampMs(fresh);
        if (baseMs && freshMs) return baseMs <= freshMs ? base : fresh;
        if (!base.isNull() && !base.isUndefined() && !(base.isString() && base.toString().isEmpty())) return base;
        return fresh;
    }
    if (key == QLatin1String("lastUsedAt")) {
        const auto baseMs = timestampMs(base);
        const auto freshMs = timestampMs(fresh);
        if (baseMs && freshMs) return baseMs >= freshMs ? base : fresh;
        if (!base.isNull() && !base.isUndefined() && !(base.isString() && base.toString().isEmpty())) return base;
        return fresh;
    }

    QJsonValue sample = !base.isUndefined() && !base.isNull() ? base
                        : !fresh.isUndefined() && !fresh.isNull() ? fresh
                                                                  : anchor;
    if (sample.isDouble() || base.isDouble() || fresh.isDouble() || anchor.isDouble()) {
        return std::max(0.0, asNumber(base) + asNumber(fresh) - asNumber(anchor));
    }
    if (sample.isString()) {
        return (!base.isUndefined() && !base.isNull()) ? base : fresh;
    }
    if (sample.isBool()) {
        return (!base.isUndefined() && !base.isNull()) ? base : fresh;
    }
    if (sample.isObject() || sample.isArray()) {
        QSet<QString> keys;
        auto take = [&](const QJsonValue &v) {
            if (v.isObject()) {
                const auto o = v.toObject();
                for (auto it = o.begin(); it != o.end(); ++it) keys.insert(it.key());
            } else if (v.isArray()) {
                const auto a = v.toArray();
                for (int i = 0; i < a.size(); ++i) keys.insert(QString::number(i));
            }
        };
        take(base);
        take(fresh);
        take(anchor);
        QStringList sorted(keys.begin(), keys.end());
        std::sort(sorted.begin(), sorted.end());
        if (sample.isArray()) {
            QJsonArray out;
            int maxIndex = 0;
            for (const auto &k : sorted) maxIndex = std::max(maxIndex, k.toInt());
            out = QJsonArray{};
            QJsonObject result;
            for (const auto &child : sorted) {
                result.insert(child, deltaValue(
                    base.isObject() ? base.toObject().value(child) : (base.isArray() ? base.toArray().at(child.toInt()) : QJsonValue()),
                    fresh.isObject() ? fresh.toObject().value(child) : (fresh.isArray() ? fresh.toArray().at(child.toInt()) : QJsonValue()),
                    anchor.isObject() ? anchor.toObject().value(child) : (anchor.isArray() ? anchor.toArray().at(child.toInt()) : QJsonValue()),
                    child));
            }
            return result;
        }
        QJsonObject result;
        const auto baseObj = base.toObject();
        const auto freshObj = fresh.toObject();
        const auto anchorObj = anchor.toObject();
        for (const auto &child : sorted) {
            result.insert(child, deltaValue(baseObj.value(child), freshObj.value(child), anchorObj.value(child), child));
        }
        return result;
    }
    return (!base.isUndefined() && !base.isNull()) ? base : fresh;
}

QJsonValue applyPeriodDelta(const QJsonValue &base, const QJsonValue &freshToday, const QJsonValue &anchorToday)
{
    return deltaValue(base, freshToday, anchorToday, {});
}

} // namespace tmon
