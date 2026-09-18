#include "core/limits/LimitsBurnRate.h"

#include <QDateTime>
#include <QJsonArray>
#include <QStringList>
#include <QtMath>
#include <cmath>
#include <optional>
#include <random>

namespace tmon {
namespace {

QString text(const QJsonValue &value)
{
    return value.toString().trimmed();
}

QString windowKey(const QJsonObject &window)
{
    return text(window.value(QStringLiteral("kind"))) + QLatin1Char(':')
        + text(window.value(QStringLiteral("metric"))) + QLatin1Char(':')
        + text(window.value(QStringLiteral("label")));
}

std::optional<double> measurableWindow(const QJsonObject &window)
{
    if (text(window.value(QStringLiteral("metric"))) == QLatin1String("credits"))
        return std::nullopt;
    const auto used = window.value(QStringLiteral("usedPercent"));
    if (!used.isDouble()) return std::nullopt;
    const double n = used.toDouble();
    if (!std::isfinite(n)) return std::nullopt;
    return n;
}

} // namespace

QString providerIdentityKey(const QJsonObject &provider)
{
    return QStringList{
        text(provider.value(QStringLiteral("provider"))),
        text(provider.value(QStringLiteral("accountKey"))),
        text(provider.value(QStringLiteral("accountEmail"))),
        text(provider.value(QStringLiteral("accountLabel")))
    }.join(QLatin1Char(':'));
}

bool isRetryableLimitStatus(const QString &status)
{
    return status == QLatin1String("timeout")
        || status == QLatin1String("rateLimited")
        || status == QLatin1String("sourceRateLimited")
        || status == QLatin1String("unavailable")
        || status == QLatin1String("error");
}

int computeRetryDelayMs(int attempt)
{
    const int exponent = qBound(0, qMax(0, attempt - 1), 30);
    const double cap = qMin(double(kLimitsRetryMaxMs), double(kLimitsRetryBaseMs) * std::pow(2.0, exponent));
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return int(std::ceil((cap / 2.0) + dist(rng) * (cap / 2.0)));
}

void markLimitsProbeSuccess(LimitsBurnState &state, const QJsonObject &row)
{
    state.live.insert(providerIdentityKey(row));
}

void recordLimitsSample(LimitsBurnState &state, const QJsonObject &limits, qint64 nowMs)
{
    for (const auto &item : limits.value(QStringLiteral("providers")).toArray()) {
        const auto provider = item.toObject();
        if (text(provider.value(QStringLiteral("status"))) != QLatin1String("ok")) continue;
        const auto updatedAt = text(provider.value(QStringLiteral("updatedAt")));
        if (updatedAt.isEmpty()) continue;
        const auto identity = providerIdentityKey(provider);
        if (!state.live.contains(identity)) continue;
        for (const auto &winV : provider.value(QStringLiteral("windows")).toArray()) {
            const auto window = winV.toObject();
            const auto used = measurableWindow(window);
            if (!used) continue;
            const auto key = identity + QLatin1Char('|') + windowKey(window);
            const auto resetsAt = text(window.value(QStringLiteral("resetsAt")));
            const auto previous = state.windows.value(key);
            if (previous.at == 0) {
                state.windows.insert(key, LimitsBurnState::Sample{*used, nowMs, updatedAt, resetsAt, 0});
                continue;
            }
            if (previous.updatedAt == updatedAt) continue;
            const qint64 elapsedMs = nowMs - previous.at;
            const bool reset = resetsAt != previous.resetsAt || *used < previous.usedPercent;
            double rate = previous.rate;
            if (!reset && elapsedMs > 0) {
                const double instant = (*used - previous.usedPercent) / double(elapsedMs);
                rate = instant >= previous.rate
                    ? instant
                    : (kLimitsUrgencyReleaseWeight * instant) + ((1.0 - kLimitsUrgencyReleaseWeight) * previous.rate);
            }
            state.windows.insert(key, LimitsBurnState::Sample{*used, nowMs, updatedAt, resetsAt, rate});
        }
    }
}

int suggestedAdaptiveMs(const LimitsBurnState &state, int baseMs)
{
    int next = baseMs > 0 ? baseMs : kLimitsAdaptiveBaseMs;
    for (auto it = state.windows.begin(); it != state.windows.end(); ++it) {
        if (it->rate <= 0) continue;
        const double remaining = qMax(0.0, 100.0 - it->usedPercent);
        const double ttlMs = remaining / it->rate;
        const int delay = int(ttlMs / double(kLimitsUrgencySamplesAhead));
        if (delay > 0) next = qMin(next, delay);
    }
    return qMax(kLimitsUrgencyFloorMs, qMin(baseMs > 0 ? baseMs : kLimitsAdaptiveBaseMs, next));
}

} // namespace tmon
