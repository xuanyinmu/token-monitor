#include "core/usage/UsageNormalize.h"

#include "core/io/HashKey.h"
#include "core/tmon.h"
#include "core/usage/JsonUtil.h"

#include <QDate>
#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QRegularExpression>
#include <QTime>
#include <QTimeZone>
#include <cmath>

namespace tmon {
namespace {

bool looksLikeUsageRow(const QJsonObject &obj)
{
    if (obj.isEmpty()) return false;
    const auto client = detectClient(obj);
    if (tokenValueForClient(obj, client) == 0 && costValue(obj) == 0) return false;
    return obj.contains(QStringLiteral("client")) || obj.contains(QStringLiteral("clients"))
        || obj.contains(QStringLiteral("source")) || obj.contains(QStringLiteral("platform"))
        || obj.contains(QStringLiteral("agent")) || obj.contains(QStringLiteral("tool"))
        || obj.contains(QStringLiteral("model")) || obj.contains(QStringLiteral("provider"))
        || obj.contains(QStringLiteral("date")) || obj.contains(QStringLiteral("name"))
        || !firstString(obj, kSessionIdKeys).isEmpty();
}

void collectUsageRows(const QJsonValue &node, QJsonArray &rows)
{
    if (node.isArray()) {
        const auto arr = node.toArray();
        for (const auto &item : arr) collectUsageRows(item, rows);
        return;
    }
    if (!node.isObject()) return;
    const auto obj = node.toObject();
    if (looksLikeUsageRow(obj)) {
        rows.append(obj);
        return;
    }
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.value().isArray() || it.value().isObject()) collectUsageRows(it.value(), rows);
    }
}

void addUsageRowToPeriod(QJsonObject &period, const QJsonObject &row)
{
    const auto client = detectClient(row);
    const double tokens = tokenValueForClient(row, client);
    const double cost = costValue(row);
    const double cacheRead = qMax(0.0, std::round(firstNumber(row, kCacheReadKeys)));
    const double cacheWrite = qMax(0.0, std::round(firstNumber(row, kCacheWriteKeys)));
    const double output = qMax(0.0, std::round(outputValueForClient(row, client)));
    const auto performance = row.value(QStringLiteral("performance")).toObject();
    const double timedTokens = qMax(0.0, std::round(firstNumber(performance, kTimedTokenKeys)));
    const double timedDurationMs = qMax(0.0, std::round(firstNumber(performance, kTimedDurationKeys)));
    const double timedOutputTokens = timedDurationMs > 0 ? output : 0;
    QString model = detectModel(row, client);
    if (client == QLatin1String("cursor") && model == QLatin1String("auto")) model = QStringLiteral("cursor-auto");

    addNumber(period, QStringLiteral("totalTokens"), qMax(0.0, std::round(tokens)));
    addNumber(period, QStringLiteral("costUsd"), cost);
    addNumber(period, QStringLiteral("cacheReadTokens"), cacheRead);
    addNumber(period, QStringLiteral("cacheWriteTokens"), cacheWrite);
    addNumber(period, QStringLiteral("outputTokens"), output);
    addNumber(period, QStringLiteral("timedTokens"), timedTokens);
    addNumber(period, QStringLiteral("timedOutputTokens"), timedOutputTokens);
    addNumber(period, QStringLiteral("timedDurationMs"), timedDurationMs);
    if (!client.isEmpty() && tokens > 0) {
        addMapNumber(period, QStringLiteral("clients"), client, std::round(tokens));
        if (cacheRead > 0) addMapNumber(period, QStringLiteral("clientCacheReads"), client, cacheRead);
        if (cacheWrite > 0) addMapNumber(period, QStringLiteral("clientCacheWrites"), client, cacheWrite);
        if (output > 0) addMapNumber(period, QStringLiteral("clientOutputs"), client, output);
    }
    if (!client.isEmpty() && cost > 0) addMapNumber(period, QStringLiteral("clientCosts"), client, cost);
    if (!model.isEmpty() && tokens > 0) {
        addMapNumber(period, QStringLiteral("models"), model, std::round(tokens));
        if (cacheRead > 0) addMapNumber(period, QStringLiteral("modelCacheReads"), model, cacheRead);
        if (cacheWrite > 0) addMapNumber(period, QStringLiteral("modelCacheWrites"), model, cacheWrite);
        if (output > 0) addMapNumber(period, QStringLiteral("modelOutputs"), model, output);
    }
    if (!model.isEmpty() && cost > 0) addMapNumber(period, QStringLiteral("modelCosts"), model, cost);
    if (!client.isEmpty() && !model.isEmpty() && tokens > 0) {
        auto clientModels = period.value(QStringLiteral("clientModels")).toObject();
        auto inner = clientModels.value(client).toObject();
        inner.insert(model, asNumber(inner.value(model)) + std::round(tokens));
        clientModels.insert(client, inner);
        period.insert(QStringLiteral("clientModels"), clientModels);
    }
    if (!client.isEmpty() && !model.isEmpty() && cost > 0) {
        auto clientModelCosts = period.value(QStringLiteral("clientModelCosts")).toObject();
        auto inner = clientModelCosts.value(client).toObject();
        inner.insert(model, asNumber(inner.value(model)) + cost);
        clientModelCosts.insert(client, inner);
        period.insert(QStringLiteral("clientModelCosts"), clientModelCosts);
    }

    const auto sessionId = firstString(row, kSessionIdKeys);
    if (!client.isEmpty() && !sessionId.isEmpty()) {
        const auto key = client + QLatin1Char(':') + sessionId;
        auto sessions = period.value(QStringLiteral("sessions")).toObject();
        auto session = sessions.value(key).toObject();
        if (session.isEmpty()) {
            session = QJsonObject{
                {QStringLiteral("client"), client},
                {QStringLiteral("sessionId"), sessionId},
                {QStringLiteral("totalTokens"), 0},
                {QStringLiteral("costUsd"), 0},
                {QStringLiteral("messageCount"), 0}
            };
        }
        addNumber(session, QStringLiteral("totalTokens"), qMax(0.0, std::round(tokens)));
        addNumber(session, QStringLiteral("costUsd"), cost);
        addNumber(session, QStringLiteral("messageCount"), qMax(0.0, std::round(firstNumber(row, kMessageCountKeys))));
        if (!model.isEmpty() && tokens > 0) {
            auto models = session.value(QStringLiteral("models")).toObject();
            models.insert(model, asNumber(models.value(model)) + std::round(tokens));
            session.insert(QStringLiteral("models"), models);
        }
        const auto started = normalizeIso(row.value(QStringLiteral("startedAt")));
        const auto lastUsed = normalizeIso(row.value(QStringLiteral("lastUsedAt")));
        if (!started.isEmpty() && session.value(QStringLiteral("startedAt")).toString().isEmpty())
            session.insert(QStringLiteral("startedAt"), started);
        if (!lastUsed.isEmpty()) session.insert(QStringLiteral("lastUsedAt"), lastUsed);
        const auto title = firstString(row, {QStringLiteral("sessionTitle"), QStringLiteral("title")});
        if (!title.isEmpty()) session.insert(QStringLiteral("title"), title);
        auto projectLabel = row.value(QStringLiteral("projectLabel")).toString();
        if (projectLabel.isEmpty()) projectLabel = row.value(QStringLiteral("workspace")).toString();
        auto projectId = row.value(QStringLiteral("projectId")).toString();
        if (projectId.isEmpty() && !row.value(QStringLiteral("workspacePath")).toString().isEmpty()) {
            const auto identity = projectIdentity(row.value(QStringLiteral("workspacePath")).toString());
            projectId = identity.value(QStringLiteral("projectId")).toString();
            if (projectLabel.isEmpty()) projectLabel = identity.value(QStringLiteral("projectLabel")).toString();
        }
        if (!projectId.isEmpty()) session.insert(QStringLiteral("projectId"), projectId);
        if (!projectLabel.isEmpty()) session.insert(QStringLiteral("projectLabel"), projectLabel);
        sessions.insert(key, session);
        period.insert(QStringLiteral("sessions"), sessions);
        if (!projectId.isEmpty() && tokens > 0) {
            auto projects = period.value(QStringLiteral("projects")).toObject();
            auto project = projects.value(projectId).toObject();
            if (project.isEmpty()) {
                project = QJsonObject{
                    {QStringLiteral("id"), projectId},
                    {QStringLiteral("label"), projectLabel},
                    {QStringLiteral("tokens"), 0},
                    {QStringLiteral("costUsd"), 0}
                };
            }
            addNumber(project, QStringLiteral("tokens"), qMax(0.0, std::round(tokens)));
            addNumber(project, QStringLiteral("costUsd"), cost);
            if (!projectLabel.isEmpty()) project.insert(QStringLiteral("label"), projectLabel);
            projects.insert(projectId, project);
            period.insert(QStringLiteral("projects"), projects);
        }
    }
}

QJsonObject fallbackUsagePeriod(const QJsonValue &json)
{
    auto period = emptyPeriod();
    const auto obj = json.toObject();
    const double tokens = qMax(0.0, std::round(tokenValue(obj)));
    period.insert(QStringLiteral("totalTokens"), tokens);
    period.insert(QStringLiteral("costUsd"), costValue(obj));
    period.insert(QStringLiteral("unclassifiedTokens"), tokens);
    auto caps = period.value(QStringLiteral("capabilities")).toObject();
    caps.insert(QStringLiteral("tokenComponents"), tokens == 0);
    caps.insert(QStringLiteral("throughput"), tokens == 0);
    period.insert(QStringLiteral("capabilities"), caps);
    return period;
}

QString normalizeProjectPath(QString value)
{
    value = value.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (value.isEmpty()) return {};
    const bool windows = QRegularExpression(QStringLiteral("^[a-z]:/"), QRegularExpression::CaseInsensitiveOption).match(value).hasMatch()
        || value.startsWith(QLatin1String("//"));
    const bool root = value == QLatin1String("/")
        || QRegularExpression(QStringLiteral("^[a-z]:/$"), QRegularExpression::CaseInsensitiveOption).match(value).hasMatch();
    if (!root) {
        while (value.endsWith(QLatin1Char('/')) && value.size() > 1) value.chop(1);
    }
    return windows ? value.toLower() : value;
}

} // namespace

QJsonObject projectIdentity(const QString &path)
{
    const auto normalized = normalizeProjectPath(path);
    if (normalized.isEmpty()) return {};
    const bool root = normalized == QLatin1String("/")
        || QRegularExpression(QStringLiteral("^[a-z]:/$"), QRegularExpression::CaseInsensitiveOption).match(normalized).hasMatch();
    QString displayPath = path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (!root) {
        while (displayPath.endsWith(QLatin1Char('/')) && displayPath.size() > 1) displayPath.chop(1);
    }
    QString label;
    if (root) {
        label = normalized == QLatin1String("/") ? QStringLiteral("/")
                                                 : QString(normalized.left(1).toUpper() + QStringLiteral(":\\"));
    } else {
        label = displayPath.section(QLatin1Char('/'), -1);
    }
    return QJsonObject{
        {QStringLiteral("projectId"), hashKey(QStringLiteral("project"), normalized)},
        {QStringLiteral("projectLabel"), label}
    };
}

qint64 isoFromMs(const QJsonValue &value)
{
    double n = 0;
    if (value.isDouble()) n = value.toDouble();
    else if (value.isString()) n = value.toString().toDouble();
    else return 0;
    if (!std::isfinite(n) || n <= 0) return 0;
    return qint64(n);
}

QJsonValue applyTokscaleSessionMetadata(QJsonValue json, bool resolveProjects)
{
    if (!json.isObject()) return json;
    auto obj = json.toObject();
    auto rows = obj.value(QStringLiteral("entries")).toArray();
    if (rows.isEmpty()) return json;

    QHash<QString, QJsonObject> sessionMeta;
    for (const auto &entryV : obj.value(QStringLiteral("sessions")).toArray()) {
        const auto entry = entryV.toObject();
        const auto client = entry.value(QStringLiteral("client")).toString().trimmed();
        auto sessionId = entry.value(QStringLiteral("sessionId")).toString().trimmed();
        if (sessionId.isEmpty()) sessionId = entry.value(QStringLiteral("session_id")).toString().trimmed();
        if (!client.isEmpty() && !sessionId.isEmpty())
            sessionMeta.insert(client + QLatin1Char(':') + sessionId, entry);
    }
    QHash<QString, QJsonObject> identities;
    for (const auto &entryV : obj.value(QStringLiteral("workspaces")).toArray()) {
        const auto entry = entryV.toObject();
        auto key = entry.value(QStringLiteral("workspaceKey")).toString().trimmed();
        if (key.isEmpty()) key = entry.value(QStringLiteral("workspace_key")).toString().trimmed();
        if (key.isEmpty() || identities.contains(key)) continue;
        const auto path = entry.value(QStringLiteral("path")).toString().trimmed();
        if (path.isEmpty()) {
            identities.insert(key, {});
            continue;
        }
        auto identity = projectIdentity(path);
        if (identity.value(QStringLiteral("projectId")).toString().isEmpty()) {
            identities.insert(key, {});
            continue;
        }
        const auto label = entry.value(QStringLiteral("label")).toString().trimmed();
        if (!label.isEmpty()) identity.insert(QStringLiteral("projectLabel"), label);
        identity.insert(QStringLiteral("path"), path);
        identities.insert(key, identity);
    }
    if (sessionMeta.isEmpty() && identities.isEmpty()) return json;

    QJsonArray next;
    for (const auto &rowV : rows) {
        auto row = rowV.toObject();
        if (row.isEmpty()) {
            next.append(rowV);
            continue;
        }
        const auto client = row.value(QStringLiteral("client")).toString().trimmed();
        auto sessionId = row.value(QStringLiteral("sessionId")).toString().trimmed();
        if (sessionId.isEmpty()) sessionId = row.value(QStringLiteral("session_id")).toString().trimmed();
        const auto meta = (!client.isEmpty() && !sessionId.isEmpty())
            ? sessionMeta.value(client + QLatin1Char(':') + sessionId)
            : QJsonObject{};
        if (!meta.isEmpty()) {
            auto started = isoFromMs(meta.contains(QStringLiteral("firstActiveMs"))
                                         ? meta.value(QStringLiteral("firstActiveMs"))
                                         : meta.value(QStringLiteral("first_active_ms")));
            auto lastUsed = isoFromMs(meta.contains(QStringLiteral("lastActiveMs"))
                                          ? meta.value(QStringLiteral("lastActiveMs"))
                                          : meta.value(QStringLiteral("last_active_ms")));
            if (started > 0 && row.value(QStringLiteral("startedAt")).toString().isEmpty())
                row.insert(QStringLiteral("startedAt"),
                           QDateTime::fromMSecsSinceEpoch(started, QTimeZone::UTC).toString(Qt::ISODateWithMs));
            if (lastUsed > 0 && row.value(QStringLiteral("lastUsedAt")).toString().isEmpty())
                row.insert(QStringLiteral("lastUsedAt"),
                           QDateTime::fromMSecsSinceEpoch(lastUsed, QTimeZone::UTC).toString(Qt::ISODateWithMs));
            const auto title = meta.value(QStringLiteral("title")).toString().trimmed();
            if (!title.isEmpty() && row.value(QStringLiteral("sessionTitle")).toString().isEmpty())
                row.insert(QStringLiteral("sessionTitle"), title);
            const double metaMsgs = firstNumber(meta, kMessageCountKeys);
            if (metaMsgs > 0 && firstNumber(row, kMessageCountKeys) == 0)
                row.insert(QStringLiteral("messageCount"), metaMsgs);
        }
        if (resolveProjects) {
            auto workspaceKey = row.value(QStringLiteral("workspaceKey")).toString().trimmed();
            if (workspaceKey.isEmpty()) workspaceKey = row.value(QStringLiteral("workspace_key")).toString().trimmed();
            if (!workspaceKey.isEmpty() && row.value(QStringLiteral("projectId")).toString().isEmpty()) {
                const auto identity = identities.value(workspaceKey);
                if (!identity.isEmpty()) {
                    row.insert(QStringLiteral("projectId"), identity.value(QStringLiteral("projectId")));
                    row.insert(QStringLiteral("projectLabel"), identity.value(QStringLiteral("projectLabel")));
                    row.insert(QStringLiteral("workspacePath"), identity.value(QStringLiteral("path")));
                }
            }
        }
        next.append(row);
    }
    obj.insert(QStringLiteral("entries"), next);
    return obj;
}

QJsonObject extractUsageFromTokscale(const QJsonValue &json, bool resolveProjects)
{
    const auto folded = applyTokscaleSessionMetadata(json, resolveProjects);
    QJsonArray rows;
    collectUsageRows(folded, rows);
    if (rows.isEmpty() && folded.isObject()) return fallbackUsagePeriod(folded);
    auto period = emptyPeriod();
    for (const auto &row : rows) addUsageRowToPeriod(period, row.toObject());
    return period;
}

QJsonObject extractUsageBundleFromTokscale(const QJsonValue &json)
{
    return QJsonObject{{QStringLiteral("period"), extractUsageFromTokscale(json)}};
}

QJsonObject periodWindowsNow()
{
    const auto now = QDateTime::currentDateTime();
    const auto todayEnd = QDateTime(now.date().addDays(1), QTime(0, 0), now.timeZone());
    QDate nextMonth(now.date().year(), now.date().month(), 1);
    nextMonth = nextMonth.addMonths(1);
    const auto monthEnd = QDateTime(nextMonth, QTime(0, 0), now.timeZone());
    return QJsonObject{
        {QStringLiteral("timeZone"), QString::fromUtf8(now.timeZone().id())},
        {QStringLiteral("today"), QJsonObject{
            {QStringLiteral("key"), now.date().toString(Qt::ISODate)},
            {QStringLiteral("endsAt"), todayEnd.toUTC().toString(Qt::ISODateWithMs)}
        }},
        {QStringLiteral("month"), QJsonObject{
            {QStringLiteral("key"), now.date().toString(QStringLiteral("yyyy-MM"))},
            {QStringLiteral("endsAt"), monthEnd.toUTC().toString(Qt::ISODateWithMs)}
        }}
    };
}

bool isPeriodExpired(const QJsonObject &record, const QString &periodName, qint64 nowMs)
{
    if (periodName == QLatin1String("allTime")) return false;
    const auto endsAt = record.value(QStringLiteral("periodWindows")).toObject()
                            .value(periodName).toObject()
                            .value(QStringLiteral("endsAt"));
    const auto endMs = timestampMs(endsAt);
    if (endMs > 0) return nowMs >= endMs;
    return false;
}

QJsonObject normalizeDeviceRecord(const QJsonObject &record)
{
    const auto nowIso = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QJsonObject out{
        {QStringLiteral("deviceId"), record.value(QStringLiteral("deviceId")).toString(
             record.value(QStringLiteral("id")).toString(QStringLiteral("unknown")))},
        {QStringLiteral("hostname"), record.value(QStringLiteral("hostname")).toString()},
        {QStringLiteral("platform"), record.value(QStringLiteral("platform")).toString()},
        {QStringLiteral("updatedAt"), record.value(QStringLiteral("updatedAt")).toString(nowIso)},
        {QStringLiteral("receivedAt"), record.value(QStringLiteral("receivedAt")).toString(nowIso)},
        {QStringLiteral("agentVersion"), record.value(QStringLiteral("agentVersion")).toString()},
        {QStringLiteral("agentRuntime"), record.value(QStringLiteral("agentRuntime")).toString()},
        {QStringLiteral("periods"), QJsonObject{}}
    };
    if (record.contains(QStringLiteral("osName"))) out.insert(QStringLiteral("osName"), record.value(QStringLiteral("osName")));
    if (record.contains(QStringLiteral("osVersion"))) out.insert(QStringLiteral("osVersion"), record.value(QStringLiteral("osVersion")));
    if (record.contains(QStringLiteral("trackedClients"))) out.insert(QStringLiteral("trackedClients"), record.value(QStringLiteral("trackedClients")));
    if (record.contains(QStringLiteral("clientStatus"))) out.insert(QStringLiteral("clientStatus"), record.value(QStringLiteral("clientStatus")));
    if (record.contains(QStringLiteral("wslStatus"))) out.insert(QStringLiteral("wslStatus"), record.value(QStringLiteral("wslStatus")));
    if (record.contains(QStringLiteral("projectsEnabled"))) out.insert(QStringLiteral("projectsEnabled"), record.value(QStringLiteral("projectsEnabled")));
    if (record.contains(QStringLiteral("history"))) out.insert(QStringLiteral("history"), record.value(QStringLiteral("history")));
    if (record.contains(QStringLiteral("periodWindows"))) out.insert(QStringLiteral("periodWindows"), record.value(QStringLiteral("periodWindows")));
    if (record.contains(QStringLiteral("limits"))) out.insert(QStringLiteral("limits"), record.value(QStringLiteral("limits")));
    auto periods = QJsonObject{};
    const auto nested = record.value(QStringLiteral("periods")).toObject();
    for (const auto &name : kPeriods) {
        auto period = record.value(name).toObject();
        if (period.isEmpty()) period = nested.value(name).toObject();
        if (period.isEmpty()) period = emptyPeriod();
        periods.insert(name, period);
    }
    out.insert(QStringLiteral("periods"), periods);
    return out;
}

QJsonObject stripSessionTextFromDeviceRecord(const QJsonObject &record)
{
    auto out = record;
    auto stripPeriod = [](QJsonObject period) {
        auto sessions = period.value(QStringLiteral("sessions")).toObject();
        QJsonObject cleaned;
        for (auto it = sessions.begin(); it != sessions.end(); ++it) {
            auto session = it.value().toObject();
            for (const auto &field : {QStringLiteral("title"), QStringLiteral("sessionTitle"), QStringLiteral("name"),
                                      QStringLiteral("preview")}) {
                session.remove(field);
            }
            cleaned.insert(it.key(), session);
        }
        period.insert(QStringLiteral("sessions"), cleaned);
        return period;
    };
    for (const auto &name : kPeriods) {
        if (out.contains(name)) out.insert(name, stripPeriod(out.value(name).toObject()));
    }
    if (out.contains(QStringLiteral("periods"))) {
        auto periods = out.value(QStringLiteral("periods")).toObject();
        for (const auto &name : kPeriods) {
            if (periods.contains(name)) periods.insert(name, stripPeriod(periods.value(name).toObject()));
        }
        out.insert(QStringLiteral("periods"), periods);
    }
    return out;
}

QJsonObject mergeDeviceRecord(const QJsonObject &existing, const QJsonObject &incoming)
{
    auto normalizedIncoming = normalizeDeviceRecord(incoming);
    if (existing.isEmpty()) return normalizedIncoming;
    const auto normalizedExisting = normalizeDeviceRecord(existing);
    if (incoming.value(QStringLiteral("limitsOnly")).toBool()) {
        normalizedIncoming.insert(QStringLiteral("periods"), normalizedExisting.value(QStringLiteral("periods")));
        for (const auto &key : {QStringLiteral("clientStatus"), QStringLiteral("wslStatus"), QStringLiteral("periodWindows"),
                                QStringLiteral("projectsEnabled"), QStringLiteral("history")}) {
            if (!normalizedIncoming.contains(key) && normalizedExisting.contains(key))
                normalizedIncoming.insert(key, normalizedExisting.value(key));
        }
    }
    if (!incoming.contains(QStringLiteral("limits")))
        normalizedIncoming.insert(QStringLiteral("limits"), normalizedExisting.value(QStringLiteral("limits")));
    if (!incoming.contains(QStringLiteral("history")) && normalizedExisting.contains(QStringLiteral("history")))
        normalizedIncoming.insert(QStringLiteral("history"), normalizedExisting.value(QStringLiteral("history")));
    return normalizedIncoming;
}

QJsonObject aggregateDevices(const QJsonArray &devices, int staleAfterMs, qint64 nowMs)
{
    if (nowMs <= 0) nowMs = QDateTime::currentMSecsSinceEpoch();
    QJsonObject aggregate{
        {QStringLiteral("updatedAt"), QDateTime::fromMSecsSinceEpoch(nowMs, QTimeZone::UTC).toString(Qt::ISODateWithMs)},
        {QStringLiteral("periods"), QJsonObject{
            {QStringLiteral("today"), emptyPeriod()},
            {QStringLiteral("month"), emptyPeriod()},
            {QStringLiteral("allTime"), emptyPeriod()}
        }},
        {QStringLiteral("devices"), QJsonArray{}},
        {QStringLiteral("staleAfterMs"), staleAfterMs}
    };
    QJsonArray deviceList;
    auto periods = aggregate.value(QStringLiteral("periods")).toObject();
    QJsonArray allLimits;
    for (const auto &item : devices) {
        const auto normalized = normalizeDeviceRecord(item.toObject());
        const auto received = timestampMs(normalized.value(QStringLiteral("receivedAt")));
        const auto updated = timestampMs(normalized.value(QStringLiteral("updatedAt")));
        const auto ageMs = nowMs - (received ? received : updated);
        const bool stale = staleAfterMs > 0 && ageMs > staleAfterMs;
        QJsonObject summary = normalized;
        summary.insert(QStringLiteral("ageMs"), ageMs);
        summary.insert(QStringLiteral("stale"), stale);
        deviceList.append(summary);
        for (const auto &name : kPeriods) {
            if (isPeriodExpired(normalized, name, nowMs)) continue;
            periods.insert(name, addPeriodInto(periods.value(name).toObject(),
                                               normalized.value(QStringLiteral("periods")).toObject().value(name).toObject()));
        }
        const auto providers = normalized.value(QStringLiteral("limits")).toObject().value(QStringLiteral("providers")).toArray();
        for (const auto &p : providers) allLimits.append(p);
    }
    aggregate.insert(QStringLiteral("devices"), deviceList);
    aggregate.insert(QStringLiteral("periods"), periods);
    aggregate.insert(QStringLiteral("limits"), QJsonObject{
        {QStringLiteral("providers"), allLimits},
        {QStringLiteral("updatedAt"), aggregate.value(QStringLiteral("updatedAt"))}
    });
    return aggregate;
}

} // namespace tmon
