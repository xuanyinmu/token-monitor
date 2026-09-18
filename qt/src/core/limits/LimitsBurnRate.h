#pragma once

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QString>

namespace tmon {

inline constexpr int kLimitsAdaptiveBaseMs = 5 * 60 * 1000;
inline constexpr int kLimitsUrgencyFloorMs = 60 * 1000;
inline constexpr int kLimitsUrgencySamplesAhead = 4;
inline constexpr double kLimitsUrgencyReleaseWeight = 0.3;
inline constexpr int kLimitsRetryBaseMs = 5 * 1000;
inline constexpr int kLimitsRetryMaxMs = 5 * 60 * 1000;

struct LimitsBurnState {
    struct Sample {
        double usedPercent = 0;
        qint64 at = 0;
        QString updatedAt;
        QString resetsAt;
        double rate = 0;
    };
    QHash<QString, Sample> windows;
    QHash<QString, qint64> attempts;
    QSet<QString> live;
};

QString providerIdentityKey(const QJsonObject &provider);
bool isRetryableLimitStatus(const QString &status);
int computeRetryDelayMs(int attempt);
void markLimitsProbeSuccess(LimitsBurnState &state, const QJsonObject &row);
void recordLimitsSample(LimitsBurnState &state, const QJsonObject &limits, qint64 nowMs);
int suggestedAdaptiveMs(const LimitsBurnState &state, int baseMs);

} // namespace tmon
