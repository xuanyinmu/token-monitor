#include "core/usage/JsonUtil.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>
#include <QtMath>
#include <cmath>

namespace tmon {

double asNumber(const QJsonValue &value)
{
    if (value.isDouble()) return value.toDouble();
    if (value.isString()) {
        QString raw = value.toString().trimmed();
        raw.remove(QLatin1Char('$'));
        raw.remove(QLatin1Char(','));
        if (raw.isEmpty()) return 0;
        bool ok = false;
        const double n = raw.toDouble(&ok);
        return ok && std::isfinite(n) ? n : 0;
    }
    return 0;
}

double firstNumber(const QJsonObject &obj, const QStringList &keys)
{
    for (const auto &key : keys) {
        if (!obj.contains(key)) continue;
        const double n = asNumber(obj.value(key));
        if (n != 0) return n;
    }
    return 0;
}

QString firstString(const QJsonObject &obj, const QStringList &keys)
{
    for (const auto &key : keys) {
        const auto s = obj.value(key).toString().trimmed();
        if (!s.isEmpty()) return s;
    }
    return {};
}

qint64 timestampMs(const QJsonValue &value)
{
    if (value.isDouble()) {
        const double n = value.toDouble();
        if (!std::isfinite(n) || n == 0) return 0;
        return n < 20'000'000'000 ? qint64(n * 1000) : qint64(n);
    }
    const auto s = value.toString().trimmed();
    if (s.isEmpty()) return 0;
    const auto dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    return dt.isValid() ? dt.toMSecsSinceEpoch() : 0;
}

QString normalizeIso(const QJsonValue &value)
{
    const auto ms = timestampMs(value);
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC).toString(Qt::ISODateWithMs) : QString();
}

bool hasDisjointReasoning(const QString &client)
{
    return client == QLatin1String("reasonix")
        || client == QLatin1String("codex")
        || client == QLatin1String("droid")
        || client == QLatin1String("dsh");
}

double tokenValue(const QJsonObject &obj)
{
    const double direct = firstNumber(obj, kTokenKeys);
    if (direct != 0) return direct;
    double sum = 0;
    for (const auto &key : kTokenComponentKeys) {
        if (obj.contains(key)) sum += asNumber(obj.value(key));
    }
    return sum;
}

double tokenValueForClient(const QJsonObject &obj, const QString &client)
{
    const double base = tokenValue(obj);
    if (!hasDisjointReasoning(client)) return base;
    const double direct = firstNumber(obj, kTokenKeys);
    return direct != 0 ? base : base + qMax(0.0, firstNumber(obj, kReasoningKeys));
}

double outputValueForClient(const QJsonObject &obj, const QString &client)
{
    const double output = qMax(0.0, firstNumber(obj, kOutputTokenKeys));
    return hasDisjointReasoning(client) ? output + qMax(0.0, firstNumber(obj, kReasoningKeys)) : output;
}

double costValue(const QJsonObject &obj)
{
    return firstNumber(obj, kCostKeys);
}

QString normalizeClientName(const QString &value)
{
    QString raw = value.trimmed().toLower();
    raw.replace(QRegularExpression(QStringLiteral("[\\s_]+")), QStringLiteral("-"));
    if (raw.isEmpty()) return {};
    auto has = [&](const char *n) { return raw.contains(QLatin1String(n)); };
    if (has("claude")) return QStringLiteral("claude");
    if (has("codex")) return QStringLiteral("codex");
    if (has("hermes")) return QStringLiteral("hermes");
    if (has("gemini")) return QStringLiteral("gemini");
    if (has("cursor")) return QStringLiteral("cursor");
    if (has("antigravity")) return QStringLiteral("antigravity");
    if (has("kimi")) return QStringLiteral("kimi");
    if (has("qwen")) return QStringLiteral("qwen");
    if (has("grok")) return QStringLiteral("grok");
    if (raw == QLatin1String("droid")) return QStringLiteral("droid");
    if (has("copilot")) return QStringLiteral("copilot");
    if (QRegularExpression(QStringLiteral("\\bpi\\b")).match(raw).hasMatch()) return QStringLiteral("pi");
    if (has("zed")) return QStringLiteral("zed");
    if (QRegularExpression(QStringLiteral("^kilo[\\s_-]*code$")).match(raw).hasMatch()) return QStringLiteral("kilo");
    if (QRegularExpression(QStringLiteral("command[\\s_-]*code")).match(raw).hasMatch()) return QStringLiteral("commandcode");
    if (has("micode")) return QStringLiteral("micode");
    if (has("zcode")) return QStringLiteral("zcode");
    if (has("kiro")) return QStringLiteral("kiro");
    if (has("codebuddy")) return QStringLiteral("codebuddy");
    if (has("workbuddy")) return QStringLiteral("workbuddy");
    if (has("proma")) return QStringLiteral("proma");
    if (has("qodercn") || raw == QLatin1String("qoder-cn") || raw == QLatin1String("qoder cn")) return QStringLiteral("qodercn");
    if (has("reasonix")) return QStringLiteral("reasonix");
    if (QRegularExpression(QStringLiteral("cherry[\\s_-]*studio")).match(raw).hasMatch()) return QStringLiteral("cherrystudio");
    if (QRegularExpression(QStringLiteral("lm[\\s_-]*studio")).match(raw).hasMatch()) return QStringLiteral("lmstudio");
    if (QRegularExpression(QStringLiteral("^unsloth(?:[\\s_-]+(?:studio|api))?$")).match(raw).hasMatch()) return QStringLiteral("unsloth");
    if (has("dsh")) return QStringLiteral("dsh");
    if (has("opencode")) return QStringLiteral("opencode");
    if (has("openclaw") || has("clawd") || has("moltbot") || has("moldbot")) return QStringLiteral("openclaw");
    raw.replace(QRegularExpression(QStringLiteral("[^a-z0-9_-]+")), QStringLiteral("-"));
    while (raw.startsWith(QLatin1Char('-')) || raw.startsWith(QLatin1Char('_'))) raw.remove(0, 1);
    while (raw.endsWith(QLatin1Char('-')) || raw.endsWith(QLatin1Char('_'))) raw.chop(1);
    return raw;
}

QString detectClient(const QJsonObject &obj)
{
    for (const auto &key : {QStringLiteral("client"), QStringLiteral("clients"), QStringLiteral("source"),
                            QStringLiteral("platform"), QStringLiteral("agent"), QStringLiteral("tool"), QStringLiteral("name")}) {
        const auto name = normalizeClientName(obj.value(key).toString());
        if (!name.isEmpty()) return name;
    }
    return {};
}

QString detectModel(const QJsonObject &obj, const QString &client)
{
    QString raw;
    for (const auto &key : {QStringLiteral("model"), QStringLiteral("modelName"), QStringLiteral("model_name"),
                            QStringLiteral("deployment"), QStringLiteral("engine")}) {
        raw = obj.value(key).toString().trimmed().toLower();
        if (!raw.isEmpty()) break;
    }
    if (raw.isEmpty()) return {};
    if (client == QLatin1String("reasonix")) {
        const auto m = QRegularExpression(QStringLiteral("^(?:deepseek|deepseek-flash)/(.+)$")).match(raw);
        if (m.hasMatch()) raw = m.captured(1);
    }
    return raw;
}

QJsonObject emptyPeriod()
{
    return QJsonObject{
        {QStringLiteral("capabilities"), QJsonObject{{QStringLiteral("tokenComponents"), true}, {QStringLiteral("throughput"), true}}},
        {QStringLiteral("totalTokens"), 0},
        {QStringLiteral("costUsd"), 0},
        {QStringLiteral("cacheReadTokens"), 0},
        {QStringLiteral("cacheWriteTokens"), 0},
        {QStringLiteral("outputTokens"), 0},
        {QStringLiteral("unclassifiedTokens"), 0},
        {QStringLiteral("timedTokens"), 0},
        {QStringLiteral("timedOutputTokens"), 0},
        {QStringLiteral("timedDurationMs"), 0},
        {QStringLiteral("clients"), QJsonObject{}},
        {QStringLiteral("clientCosts"), QJsonObject{}},
        {QStringLiteral("clientCacheReads"), QJsonObject{}},
        {QStringLiteral("clientCacheWrites"), QJsonObject{}},
        {QStringLiteral("clientOutputs"), QJsonObject{}},
        {QStringLiteral("clientUnclassifiedTokens"), QJsonObject{}},
        {QStringLiteral("models"), QJsonObject{}},
        {QStringLiteral("modelCosts"), QJsonObject{}},
        {QStringLiteral("modelCacheReads"), QJsonObject{}},
        {QStringLiteral("modelCacheWrites"), QJsonObject{}},
        {QStringLiteral("modelOutputs"), QJsonObject{}},
        {QStringLiteral("modelUnclassifiedTokens"), QJsonObject{}},
        {QStringLiteral("clientModels"), QJsonObject{}},
        {QStringLiteral("clientModelCosts"), QJsonObject{}},
        {QStringLiteral("projects"), QJsonObject{}},
        {QStringLiteral("sessions"), QJsonObject{}}
    };
}

void addNumber(QJsonObject &obj, const QString &key, double delta)
{
    obj.insert(key, asNumber(obj.value(key)) + delta);
}

void addMapNumber(QJsonObject &obj, const QString &mapKey, const QString &item, double delta)
{
    if (item.isEmpty() || delta == 0) return;
    auto map = obj.value(mapKey).toObject();
    map.insert(item, asNumber(map.value(item)) + delta);
    obj.insert(mapKey, map);
}

QJsonObject addPeriodInto(QJsonObject target, const QJsonObject &source)
{
    auto caps = target.value(QStringLiteral("capabilities")).toObject();
    const auto srcCaps = source.value(QStringLiteral("capabilities")).toObject();
    caps.insert(QStringLiteral("tokenComponents"),
                caps.value(QStringLiteral("tokenComponents")).toBool(true)
                    && srcCaps.value(QStringLiteral("tokenComponents")).toBool(true));
    caps.insert(QStringLiteral("throughput"),
                caps.value(QStringLiteral("throughput")).toBool(true)
                    && srcCaps.value(QStringLiteral("throughput")).toBool(true));
    target.insert(QStringLiteral("capabilities"), caps);

    for (const auto &key : {QStringLiteral("totalTokens"), QStringLiteral("costUsd"), QStringLiteral("cacheReadTokens"),
                            QStringLiteral("cacheWriteTokens"), QStringLiteral("outputTokens"), QStringLiteral("unclassifiedTokens"),
                            QStringLiteral("timedTokens"), QStringLiteral("timedOutputTokens"), QStringLiteral("timedDurationMs")}) {
        addNumber(target, key, asNumber(source.value(key)));
    }

    const auto mergeMap = [&](const QString &key) {
        auto map = target.value(key).toObject();
        const auto src = source.value(key).toObject();
        for (auto it = src.begin(); it != src.end(); ++it)
            map.insert(it.key(), asNumber(map.value(it.key())) + asNumber(it.value()));
        target.insert(key, map);
    };
    for (const auto &key : {QStringLiteral("clients"), QStringLiteral("clientCosts"), QStringLiteral("clientCacheReads"),
                            QStringLiteral("clientCacheWrites"), QStringLiteral("clientOutputs"),
                            QStringLiteral("clientUnclassifiedTokens"), QStringLiteral("models"), QStringLiteral("modelCosts"),
                            QStringLiteral("modelCacheReads"), QStringLiteral("modelCacheWrites"), QStringLiteral("modelOutputs"),
                            QStringLiteral("modelUnclassifiedTokens")}) {
        mergeMap(key);
    }

    auto clientModels = target.value(QStringLiteral("clientModels")).toObject();
    const auto srcClientModels = source.value(QStringLiteral("clientModels")).toObject();
    for (auto it = srcClientModels.begin(); it != srcClientModels.end(); ++it) {
        auto inner = clientModels.value(it.key()).toObject();
        const auto srcInner = it.value().toObject();
        for (auto m = srcInner.begin(); m != srcInner.end(); ++m)
            inner.insert(m.key(), asNumber(inner.value(m.key())) + asNumber(m.value()));
        clientModels.insert(it.key(), inner);
    }
    target.insert(QStringLiteral("clientModels"), clientModels);

    auto clientModelCosts = target.value(QStringLiteral("clientModelCosts")).toObject();
    const auto srcCosts = source.value(QStringLiteral("clientModelCosts")).toObject();
    for (auto it = srcCosts.begin(); it != srcCosts.end(); ++it) {
        auto inner = clientModelCosts.value(it.key()).toObject();
        const auto srcInner = it.value().toObject();
        for (auto m = srcInner.begin(); m != srcInner.end(); ++m)
            inner.insert(m.key(), asNumber(inner.value(m.key())) + asNumber(m.value()));
        clientModelCosts.insert(it.key(), inner);
    }
    target.insert(QStringLiteral("clientModelCosts"), clientModelCosts);

    auto projects = target.value(QStringLiteral("projects")).toObject();
    const auto srcProjects = source.value(QStringLiteral("projects")).toObject();
    for (auto it = srcProjects.begin(); it != srcProjects.end(); ++it) {
        auto proj = projects.value(it.key()).toObject();
        const auto src = it.value().toObject();
        if (proj.isEmpty()) proj.insert(QStringLiteral("label"), src.value(QStringLiteral("label")));
        addNumber(proj, QStringLiteral("tokens"), asNumber(src.value(QStringLiteral("tokens"))));
        addNumber(proj, QStringLiteral("costUsd"), asNumber(src.value(QStringLiteral("costUsd"))));
        auto clients = proj.value(QStringLiteral("clients")).toObject();
        const auto srcClients = src.value(QStringLiteral("clients")).toObject();
        for (auto c = srcClients.begin(); c != srcClients.end(); ++c)
            clients.insert(c.key(), asNumber(clients.value(c.key())) + asNumber(c.value()));
        proj.insert(QStringLiteral("clients"), clients);
        projects.insert(it.key(), proj);
    }
    target.insert(QStringLiteral("projects"), projects);

    auto sessions = target.value(QStringLiteral("sessions")).toObject();
    const auto srcSessions = source.value(QStringLiteral("sessions")).toObject();
    for (auto it = srcSessions.begin(); it != srcSessions.end(); ++it) {
        sessions.insert(it.key(), it.value());
    }
    target.insert(QStringLiteral("sessions"), sessions);
    return target;
}

QJsonObject mergePeriods(const QJsonObject &a, const QJsonObject &b)
{
    return addPeriodInto(addPeriodInto(emptyPeriod(), a), b);
}

} // namespace tmon
