#include "core/usage/ArchiveUsage.h"

#include <QDate>
#include <QJsonObject>
#include <QSet>
#include <cmath>

namespace tmon {

namespace {

double numberValue(const QJsonValue &value)
{
    return value.isDouble() ? value.toDouble() : 0.0;
}

QString normalizeClientId(const QString &value)
{
    return value.trimmed().toLower();
}

QString localDay()
{
    return QDate::currentDate().toString(Qt::ISODate);
}

QString localMonth()
{
    return QDate::currentDate().toString(QStringLiteral("yyyy-MM"));
}

bool isReasonixSyntheticSession(const QJsonObject &session, const QString &key = QString())
{
    // reasonix/sessionGuard: renderer-only view; any Reasonix-shaped entry on
    // the period.sessions channel is untrusted and dropped.
    const auto client = session.value(QStringLiteral("client")).toString().trimmed().toLower();
    const auto sessionId = session.value(QStringLiteral("sessionId"))
                               .toString(session.value(QStringLiteral("session_id")).toString())
                               .trimmed().toLower();
    const auto sessionKey = key.trimmed().toLower();
    return client == QLatin1String("reasonix")
        || client == QLatin1String("reasonix-stats")
        || sessionId.startsWith(QLatin1String("reasonix-stats:"))
        || sessionId.startsWith(QLatin1String("reasonix:"))
        || sessionKey.startsWith(QLatin1String("reasonix:"))
        || sessionKey.contains(QLatin1String("reasonix-stats:"));
}

} // namespace

// --- archived client usage (clientUsageArchive.js) ---

namespace {

void addClientUsage(QJsonObject &period, const QString &client, const QJsonObject &usage)
{
    const double tokens = std::max(0.0, std::round(numberValue(usage.value(QStringLiteral("totalTokens")))));
    const double cost = numberValue(usage.value(QStringLiteral("costUsd")));
    period.insert(QStringLiteral("totalTokens"), numberValue(period.value(QStringLiteral("totalTokens"))) + tokens);
    period.insert(QStringLiteral("costUsd"), numberValue(period.value(QStringLiteral("costUsd"))) + cost);
    if (tokens > 0) {
        auto clients = period.value(QStringLiteral("clients")).toObject();
        clients.insert(client, numberValue(clients.value(client)) + tokens);
        period.insert(QStringLiteral("clients"), clients);
    }
    if (cost > 0) {
        auto clientCosts = period.value(QStringLiteral("clientCosts")).toObject();
        clientCosts.insert(client, numberValue(clientCosts.value(client)) + cost);
        period.insert(QStringLiteral("clientCosts"), clientCosts);
    }
    const auto usageModels = usage.value(QStringLiteral("models")).toObject();
    auto models = period.value(QStringLiteral("models")).toObject();
    auto clientModels = period.value(QStringLiteral("clientModels")).toObject();
    auto clientModelRow = clientModels.value(client).toObject();
    for (auto it = usageModels.begin(); it != usageModels.end(); ++it) {
        const double modelTokens = std::max(0.0, std::round(numberValue(it.value())));
        models.insert(it.key(), numberValue(models.value(it.key())) + modelTokens);
        clientModelRow.insert(it.key(), numberValue(clientModelRow.value(it.key())) + modelTokens);
    }
    if (!usageModels.isEmpty()) {
        period.insert(QStringLiteral("models"), models);
        clientModels.insert(client, clientModelRow);
        period.insert(QStringLiteral("clientModels"), clientModels);
    }
}

bool archivedHasUsage(const QJsonObject &period)
{
    if (numberValue(period.value(QStringLiteral("totalTokens"))) > 0
        || numberValue(period.value(QStringLiteral("costUsd"))) > 0)
        return true;
    const auto sessions = period.value(QStringLiteral("sessions")).toObject();
    for (auto it = sessions.begin(); it != sessions.end(); ++it) {
        const auto session = it.value().toObject();
        if (numberValue(session.value(QStringLiteral("totalTokens"))) > 0
            || numberValue(session.value(QStringLiteral("costUsd"))) > 0)
            return true;
    }
    return false;
}

} // namespace

void applyArchivedClientUsageToRecord(QJsonObject &record, const QJsonObject &archive,
                                      const QStringList &activeClients)
{
    const auto source = archive.contains(QStringLiteral("clients"))
        ? archive.value(QStringLiteral("clients")).toObject()
        : archive;
    QSet<QString> active;
    for (const auto &id : activeClients)
        active.insert(normalizeClientId(id));

    const QString todayKey = localDay();
    const QString monthKey = localMonth();
    bool changed = false;
    for (auto it = source.begin(); it != source.end(); ++it) {
        const auto entry = it.value().toObject();
        const auto client = normalizeClientId(entry.value(QStringLiteral("client")).toString(it.key()));
        if (client.isEmpty() || active.contains(client)) continue;
        const QString day = entry.value(QStringLiteral("day")).toString();
        const QString month = entry.value(QStringLiteral("month")).toString();
        const auto periods = entry.value(QStringLiteral("periods")).toObject();
        for (const QString periodName : { QStringLiteral("today"), QStringLiteral("month"), QStringLiteral("allTime") }) {
            const auto usage = periods.value(periodName).toObject();
            if (!archivedHasUsage(usage)) continue;
            if (periodName == QLatin1String("today") && day != todayKey) continue;
            if (periodName == QLatin1String("month") && month != monthKey) continue;
            if (!record.value(periodName).isObject()) continue;
            auto period = record.value(periodName).toObject();
            addClientUsage(period, client, usage);
            record.insert(periodName, period);
            changed = true;
        }
    }
    if (changed)
        record.insert(QStringLiteral("periods"), QJsonObject{
            {QStringLiteral("today"), record.value(QStringLiteral("today"))},
            {QStringLiteral("month"), record.value(QStringLiteral("month"))},
            {QStringLiteral("allTime"), record.value(QStringLiteral("allTime"))}
        });
}

// --- retained session archive (sessionUsageArchive.js) ---

namespace {

void addMap(QJsonObject &period, const char *field, const QString &key, double value)
{
    if (value == 0) return;
    auto map = period.value(QLatin1String(field)).toObject();
    map.insert(key, numberValue(map.value(key)) + value);
    period.insert(QLatin1String(field), map);
}

void addSessionBreakdown(QJsonObject &period, const QJsonObject &session)
{
    const auto client = session.value(QStringLiteral("client")).toString();
    const double cacheRead = std::max(0.0, std::round(numberValue(session.value(QStringLiteral("cacheReadTokens")))));
    const double cacheWrite = std::max(0.0, std::round(numberValue(session.value(QStringLiteral("cacheWriteTokens")))));
    const double output = std::max(0.0, std::round(numberValue(session.value(QStringLiteral("outputTokens")))));
    if (cacheRead > 0) addMap(period, "clientCacheReads", client, cacheRead);
    if (cacheWrite > 0) addMap(period, "clientCacheWrites", client, cacheWrite);
    if (output > 0) addMap(period, "clientOutputs", client, output);

    const auto sessionModels = session.value(QStringLiteral("models")).toObject();
    QList<QPair<QString, double>> modelTokens;
    double totalModelTokens = 0;
    for (auto it = sessionModels.begin(); it != sessionModels.end(); ++it) {
        const double t = numberValue(it.value());
        if (t > 0) {
            modelTokens.append({it.key(), t});
            totalModelTokens += t;
        }
    }
    if (totalModelTokens == 0) return;
    if (modelTokens.size() > 1) {
        // Electron addSessionBreakdown: multi-model archived sessions cannot be
        // attributed exactly, so every token lands in the unclassified bucket.
        for (const auto &[model, tokens] : modelTokens)
            addMap(period, "modelUnclassifiedTokens", model, tokens);
        auto capabilities = period.value(QStringLiteral("capabilities")).toObject();
        capabilities.insert(QStringLiteral("tokenComponents"), false);
        period.insert(QStringLiteral("capabilities"), capabilities);
        return;
    }
    for (const auto &[model, tokens] : modelTokens) {
        const double cr = std::min(tokens, cacheRead);
        const double cw = std::min(tokens - cr, cacheWrite);
        const double ou = std::min(tokens - cr - cw, output);
        if (cr > 0) addMap(period, "modelCacheReads", model, cr);
        if (cw > 0) addMap(period, "modelCacheWrites", model, cw);
        if (ou > 0) addMap(period, "modelOutputs", model, ou);
        const double unclassified = std::max(0.0, tokens - cr - cw - ou);
        if (unclassified > 0) {
            addMap(period, "modelUnclassifiedTokens", model, unclassified);
            auto capabilities = period.value(QStringLiteral("capabilities")).toObject();
            capabilities.insert(QStringLiteral("tokenComponents"), false);
            period.insert(QStringLiteral("capabilities"), capabilities);
        }
    }
}

void addArchivedSession(QJsonObject &period, const QJsonObject &session, const QString &key)
{
    if (isReasonixSyntheticSession(session, key)) return;
    const auto sessionKey = session.value(QStringLiteral("client")).toString()
        + QLatin1Char(':') + session.value(QStringLiteral("sessionId")).toString();
    auto sessions = period.value(QStringLiteral("sessions")).toObject();
    if (sessionKey.isEmpty() || sessions.contains(sessionKey)) return;

    auto archived = session;
    archived.insert(QStringLiteral("archived"), true);
    sessions.insert(sessionKey, archived);
    period.insert(QStringLiteral("sessions"), sessions);

    const double tokens = std::max(0.0, std::round(numberValue(archived.value(QStringLiteral("totalTokens")))));
    const double cost = numberValue(archived.value(QStringLiteral("costUsd")));
    const double cacheRead = std::max(0.0, std::round(numberValue(archived.value(QStringLiteral("cacheReadTokens")))));
    const double cacheWrite = std::max(0.0, std::round(numberValue(archived.value(QStringLiteral("cacheWriteTokens")))));
    const double output = std::max(0.0, std::round(numberValue(archived.value(QStringLiteral("outputTokens")))));
    const auto client = archived.value(QStringLiteral("client")).toString();

    period.insert(QStringLiteral("totalTokens"), numberValue(period.value(QStringLiteral("totalTokens"))) + tokens);
    period.insert(QStringLiteral("costUsd"), numberValue(period.value(QStringLiteral("costUsd"))) + cost);
    period.insert(QStringLiteral("cacheReadTokens"), numberValue(period.value(QStringLiteral("cacheReadTokens"))) + cacheRead);
    period.insert(QStringLiteral("cacheWriteTokens"), numberValue(period.value(QStringLiteral("cacheWriteTokens"))) + cacheWrite);
    period.insert(QStringLiteral("outputTokens"), numberValue(period.value(QStringLiteral("outputTokens"))) + output);
    const double unclassified = std::max(0.0, tokens - cacheRead - cacheWrite - output);
    if (unclassified > 0) {
        period.insert(QStringLiteral("unclassifiedTokens"), numberValue(period.value(QStringLiteral("unclassifiedTokens"))) + unclassified);
        addMap(period, "clientUnclassifiedTokens", client, unclassified);
        auto capabilities = period.value(QStringLiteral("capabilities")).toObject();
        capabilities.insert(QStringLiteral("tokenComponents"), false);
        period.insert(QStringLiteral("capabilities"), capabilities);
    }
    if (tokens > 0) addMap(period, "clients", client, tokens);
    if (cost > 0) addMap(period, "clientCosts", client, cost);

    const auto archivedModels = archived.value(QStringLiteral("models")).toObject();
    for (auto it = archivedModels.begin(); it != archivedModels.end(); ++it) {
        const double next = std::max(0.0, std::round(numberValue(it.value())));
        if (next <= 0) continue;
        addMap(period, "models", it.key(), next);
        auto clientModels = period.value(QStringLiteral("clientModels")).toObject();
        auto row = clientModels.value(client).toObject();
        row.insert(it.key(), numberValue(row.value(it.key())) + next);
        clientModels.insert(client, row);
        period.insert(QStringLiteral("clientModels"), clientModels);
    }
    const auto archivedModelCosts = archived.value(QStringLiteral("modelCosts")).toObject();
    for (auto it = archivedModelCosts.begin(); it != archivedModelCosts.end(); ++it) {
        const double next = numberValue(it.value());
        if (next <= 0) continue;
        addMap(period, "modelCosts", it.key(), next);
        auto clientModelCosts = period.value(QStringLiteral("clientModelCosts")).toObject();
        auto row = clientModelCosts.value(client).toObject();
        row.insert(it.key(), numberValue(row.value(it.key())) + next);
        clientModelCosts.insert(client, row);
        period.insert(QStringLiteral("clientModelCosts"), clientModelCosts);
    }

    addSessionBreakdown(period, archived);
}

bool sessionHasUsage(const QJsonObject &session)
{
    return numberValue(session.value(QStringLiteral("totalTokens"))) > 0
        || numberValue(session.value(QStringLiteral("costUsd"))) > 0;
}

} // namespace

void applySessionUsageArchiveToRecord(QJsonObject &record, const QJsonObject &archive)
{
    const auto source = archive.contains(QStringLiteral("sessions"))
        ? archive.value(QStringLiteral("sessions")).toObject()
        : archive;
    const QString todayKey = localDay();
    const QString monthKey = localMonth();
    bool changed = false;
    for (auto entryIt = source.begin(); entryIt != source.end(); ++entryIt) {
        const auto entry = entryIt.value().toObject();
        if (isReasonixSyntheticSession(entry, entryIt.key())) continue;
        const auto periods = entry.value(QStringLiteral("periods")).toObject();
        const auto windows = entry.value(QStringLiteral("periodWindows")).toObject();
        for (const QString periodName : { QStringLiteral("today"), QStringLiteral("month"), QStringLiteral("allTime") }) {
            const auto session = periods.value(periodName).toObject();
            if (!sessionHasUsage(session)) continue;
            if (periodName == QLatin1String("today")) {
                const auto window = windows.value(QStringLiteral("today")).toObject();
                const QString windowDay = window.value(QStringLiteral("day")).toString(entry.value(QStringLiteral("day")).toString());
                if (windowDay != todayKey) continue;
            } else if (periodName == QLatin1String("month")) {
                const auto window = windows.value(QStringLiteral("month")).toObject();
                const QString windowMonth = window.value(QStringLiteral("month")).toString(entry.value(QStringLiteral("month")).toString());
                if (windowMonth != monthKey) continue;
            }
            if (!record.value(periodName).isObject()) continue;
            auto period = record.value(periodName).toObject();
            const qint64 before = qint64(numberValue(period.value(QStringLiteral("totalTokens"))));
            addArchivedSession(period, session, entryIt.key());
            if (qint64(numberValue(period.value(QStringLiteral("totalTokens")))) != before) {
                record.insert(periodName, period);
                changed = true;
            }
        }
    }
    if (changed)
        record.insert(QStringLiteral("periods"), QJsonObject{
            {QStringLiteral("today"), record.value(QStringLiteral("today"))},
            {QStringLiteral("month"), record.value(QStringLiteral("month"))},
            {QStringLiteral("allTime"), record.value(QStringLiteral("allTime"))}
        });
}

} // namespace tmon
