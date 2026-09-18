#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace tmon {

inline const QStringList kPeriods{QStringLiteral("today"), QStringLiteral("month"), QStringLiteral("allTime")};
inline const QStringList kTokenKeys{
    QStringLiteral("totalTokens"), QStringLiteral("total_tokens"), QStringLiteral("totalTokenCount"),
    QStringLiteral("total_token_count"), QStringLiteral("tokens"), QStringLiteral("tokenCount"), QStringLiteral("token_count")
};
inline const QStringList kTokenComponentKeys{
    QStringLiteral("input"), QStringLiteral("inputTokens"), QStringLiteral("input_tokens"),
    QStringLiteral("promptTokens"), QStringLiteral("prompt_tokens"),
    QStringLiteral("output"), QStringLiteral("outputTokens"), QStringLiteral("output_tokens"),
    QStringLiteral("completionTokens"), QStringLiteral("completion_tokens"),
    QStringLiteral("cacheRead"), QStringLiteral("cacheReadTokens"), QStringLiteral("cache_read_tokens"),
    QStringLiteral("cacheWrite"), QStringLiteral("cacheWriteTokens"), QStringLiteral("cache_write_tokens"),
    QStringLiteral("cachedTokens"), QStringLiteral("cached_tokens"),
    QStringLiteral("cacheCreationInputTokens"), QStringLiteral("cache_creation_input_tokens"),
    QStringLiteral("cacheReadInputTokens"), QStringLiteral("cache_read_input_tokens"),
    QStringLiteral("totalInput"), QStringLiteral("totalOutput"), QStringLiteral("totalCacheRead"), QStringLiteral("totalCacheWrite")
};
inline const QStringList kCostKeys{
    QStringLiteral("costUsd"), QStringLiteral("cost_usd"), QStringLiteral("costUSD"),
    QStringLiteral("cost"), QStringLiteral("totalCost"), QStringLiteral("total_cost")
};
inline const QStringList kInputTokenKeys{
    QStringLiteral("input"), QStringLiteral("inputTokens"), QStringLiteral("input_tokens"),
    QStringLiteral("promptTokens"), QStringLiteral("prompt_tokens"), QStringLiteral("totalInput")
};
inline const QStringList kOutputTokenKeys{
    QStringLiteral("output"), QStringLiteral("outputTokens"), QStringLiteral("output_tokens"),
    QStringLiteral("completionTokens"), QStringLiteral("completion_tokens"), QStringLiteral("totalOutput")
};
inline const QStringList kCacheReadKeys{
    QStringLiteral("cacheRead"), QStringLiteral("cacheReadTokens"), QStringLiteral("cache_read_tokens"),
    QStringLiteral("cachedTokens"), QStringLiteral("cached_tokens"), QStringLiteral("cacheReadInputTokens"), QStringLiteral("totalCacheRead")
};
inline const QStringList kCacheWriteKeys{
    QStringLiteral("cacheWrite"), QStringLiteral("cacheWriteTokens"), QStringLiteral("cache_write_tokens"),
    QStringLiteral("cacheCreationInputTokens"), QStringLiteral("totalCacheWrite")
};
inline const QStringList kReasoningKeys{
    QStringLiteral("reasoning"), QStringLiteral("reasoningTokens"), QStringLiteral("reasoning_tokens")
};
inline const QStringList kTimedDurationKeys{
    QStringLiteral("totalDurationMs"), QStringLiteral("total_duration_ms"),
    QStringLiteral("timedDurationMs"), QStringLiteral("timed_duration_ms")
};
inline const QStringList kTimedTokenKeys{QStringLiteral("timedTokens"), QStringLiteral("timed_tokens")};
inline const QStringList kSessionIdKeys{
    QStringLiteral("sessionId"), QStringLiteral("session_id"), QStringLiteral("session"),
    QStringLiteral("conversationId"), QStringLiteral("conversation_id"), QStringLiteral("threadId"), QStringLiteral("thread_id")
};
inline const QStringList kMessageCountKeys{
    QStringLiteral("messageCount"), QStringLiteral("message_count"), QStringLiteral("messages"),
    QStringLiteral("totalMessages"), QStringLiteral("total_messages")
};

double asNumber(const QJsonValue &value);
double firstNumber(const QJsonObject &obj, const QStringList &keys);
QString firstString(const QJsonObject &obj, const QStringList &keys);
qint64 timestampMs(const QJsonValue &value);
QString normalizeIso(const QJsonValue &value);
bool hasDisjointReasoning(const QString &client);
double tokenValue(const QJsonObject &obj);
double tokenValueForClient(const QJsonObject &obj, const QString &client);
double outputValueForClient(const QJsonObject &obj, const QString &client);
double costValue(const QJsonObject &obj);
QString normalizeClientName(const QString &value);
QString detectClient(const QJsonObject &obj);
QString detectModel(const QJsonObject &obj, const QString &client);
QJsonObject emptyPeriod();
void addNumber(QJsonObject &obj, const QString &key, double delta);
void addMapNumber(QJsonObject &obj, const QString &mapKey, const QString &item, double delta);
QJsonObject addPeriodInto(QJsonObject target, const QJsonObject &source);
QJsonObject mergePeriods(const QJsonObject &a, const QJsonObject &b);

} // namespace tmon
