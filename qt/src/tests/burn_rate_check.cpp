#include "core/limits/LimitsBurnRate.h"

#include <QJsonArray>
#include <QJsonObject>
#include <iostream>

int main()
{
    using namespace tmon;
    if (!isRetryableLimitStatus(QStringLiteral("unavailable"))) {
        std::cerr << "retryable failed\n";
        return 1;
    }
    if (isRetryableLimitStatus(QStringLiteral("ok"))) {
        std::cerr << "ok should not retry\n";
        return 1;
    }
    const int delay = computeRetryDelayMs(1);
    if (delay < 1000 || delay > kLimitsRetryMaxMs) {
        std::cerr << "retry delay out of range\n";
        return 1;
    }

    LimitsBurnState state;
    QJsonObject row{
        {QStringLiteral("provider"), QStringLiteral("deepseek")},
        {QStringLiteral("accountKey"), QStringLiteral("k")},
        {QStringLiteral("status"), QStringLiteral("ok")},
        {QStringLiteral("updatedAt"), QStringLiteral("2026-01-01T00:00:00.000Z")},
        {QStringLiteral("windows"), QJsonArray{QJsonObject{
            {QStringLiteral("kind"), QStringLiteral("session")},
            {QStringLiteral("label"), QStringLiteral("5h")},
            {QStringLiteral("usedPercent"), 10.0}
        }}}
    };
    markLimitsProbeSuccess(state, row);
    recordLimitsSample(state, QJsonObject{{QStringLiteral("providers"), QJsonArray{row}}}, 1'000);
    row.insert(QStringLiteral("updatedAt"), QStringLiteral("2026-01-01T00:01:00.000Z"));
    auto windows = row.value(QStringLiteral("windows")).toArray();
    auto w = windows.at(0).toObject();
    w.insert(QStringLiteral("usedPercent"), 50.0);
    windows.replace(0, w);
    row.insert(QStringLiteral("windows"), windows);
    recordLimitsSample(state, QJsonObject{{QStringLiteral("providers"), QJsonArray{row}}}, 61'000);
    const int adaptive = suggestedAdaptiveMs(state, kLimitsAdaptiveBaseMs);
    if (adaptive > kLimitsAdaptiveBaseMs || adaptive < kLimitsUrgencyFloorMs) {
        std::cerr << "adaptive interval unexpected: " << adaptive << "\n";
        return 1;
    }
    std::cout << "burn_rate_check ok\n";
    return 0;
}
