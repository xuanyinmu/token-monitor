#include "core/catalog/Catalog.h"

#include <QMap>
#include <QSet>

namespace tmon {
namespace {

const QVector<ClientInfo> kClients = {
    {QStringLiteral("claude"), QStringLiteral("Claude Code"), true, false},
    {QStringLiteral("codex"), QStringLiteral("Codex"), true, false},
    {QStringLiteral("opencode"), QStringLiteral("OpenCode"), true, false},
    {QStringLiteral("hermes"), QStringLiteral("Hermes Agent"), true, false},
    {QStringLiteral("openclaw"), QStringLiteral("OpenClaw"), true, false},
    {QStringLiteral("cursor"), QStringLiteral("Cursor"), true, false},
    {QStringLiteral("antigravity"), QStringLiteral("Antigravity"), true, false},
    {QStringLiteral("cline"), QStringLiteral("Cline"), true, false},
    {QStringLiteral("droid"), QStringLiteral("Factory Droid"), true, false},
    {QStringLiteral("kimi"), QStringLiteral("Kimi"), true, false},
    {QStringLiteral("qwen"), QStringLiteral("Qwen"), true, false},
    {QStringLiteral("grok"), QStringLiteral("Grok Build"), true, false},
    {QStringLiteral("copilot"), QStringLiteral("GitHub Copilot"), true, false},
    {QStringLiteral("pi"), QStringLiteral("Pi"), true, false},
    {QStringLiteral("zed"), QStringLiteral("Zed"), true, false},
    {QStringLiteral("kilo"), QStringLiteral("Kilo"), true, false},
    {QStringLiteral("commandcode"), QStringLiteral("Command Code"), true, false},
    {QStringLiteral("micode"), QStringLiteral("MiMo Code"), false, false},
    {QStringLiteral("zcode"), QStringLiteral("ZCode"), true, false},
    {QStringLiteral("kiro"), QStringLiteral("Kiro"), true, false},
    {QStringLiteral("codebuddy"), QStringLiteral("CodeBuddy"), true, false},
    {QStringLiteral("workbuddy"), QStringLiteral("WorkBuddy"), true, false},
    {QStringLiteral("proma"), QStringLiteral("Proma"), true, true},
    {QStringLiteral("qodercn"), QStringLiteral("Qoder CN"), false, true},
    {QStringLiteral("reasonix"), QStringLiteral("Reasonix"), true, false},
    {QStringLiteral("dsh"), QStringLiteral("DeepSeek Harness"), true, false},
    {QStringLiteral("cherrystudio"), QStringLiteral("Cherry Studio"), true, false},
    {QStringLiteral("lmstudio"), QStringLiteral("LM Studio"), true, false},
    {QStringLiteral("unsloth"), QStringLiteral("Unsloth"), true, false}
};

const QVector<LimitProviderInfo> kProviders = {
    {QStringLiteral("claude"), QStringLiteral("Claude"), QStringLiteral("Claude Code")},
    {QStringLiteral("codex"), QStringLiteral("Codex"), {}},
    {QStringLiteral("opencode"), QStringLiteral("OpenCode"), {}},
    {QStringLiteral("cursor"), QStringLiteral("Cursor"), {}},
    {QStringLiteral("antigravity"), QStringLiteral("Antigravity"), {}},
    {QStringLiteral("kimi"), QStringLiteral("Kimi"), {}},
    {QStringLiteral("grok"), QStringLiteral("Grok"), {}},
    {QStringLiteral("copilot"), QStringLiteral("GitHub Copilot"), {}},
    {QStringLiteral("zed"), QStringLiteral("Zed"), {}},
    {QStringLiteral("commandcode"), QStringLiteral("Command Code"), {}},
    {QStringLiteral("mimo"), QStringLiteral("MiMo"), {}},
    {QStringLiteral("zai"), QStringLiteral("GLM"), QStringLiteral("Z.ai / GLM")},
    {QStringLiteral("zaiteam"), QStringLiteral("GLM Team"), {}},
    {QStringLiteral("kiro"), QStringLiteral("Kiro"), {}},
    {QStringLiteral("workbuddy"), QStringLiteral("WorkBuddy"), {}},
    {QStringLiteral("qoder"), QStringLiteral("Qoder"), {}},
    {QStringLiteral("deepseek"), QStringLiteral("DeepSeek"), {}},
    {QStringLiteral("openrouter"), QStringLiteral("OpenRouter"), {}},
    {QStringLiteral("minimax"), QStringLiteral("Minimax"), {}},
    {QStringLiteral("volcengine"), QStringLiteral("Volcengine"), {}},
    {QStringLiteral("ollama"), QStringLiteral("Ollama"), {}},
    {QStringLiteral("trae"), QStringLiteral("Trae CN"), {}},
    {QStringLiteral("alibaba"), QStringLiteral("Alibaba Cloud"), {}},
    {QStringLiteral("thirdparty"), QStringLiteral("Third-party APIs"), {}}
};

QString joinIds(const QStringList &ids)
{
    return ids.join(QLatin1Char(','));
}

} // namespace

QVector<ClientInfo> clientCatalog() { return kClients; }

QStringList clientIds()
{
    QStringList ids;
    for (const auto &c : kClients) ids.append(c.id);
    return ids;
}

QStringList defaultClientIds()
{
    QStringList ids;
    for (const auto &c : kClients) if (c.defaultTracked) ids.append(c.id);
    return ids;
}

QStringList locallyParsedClientIds()
{
    QStringList ids;
    for (const auto &c : kClients) if (c.locallyParsed) ids.append(c.id);
    return ids;
}

QString defaultClientsCsv() { return joinIds(defaultClientIds()); }
QString knownClientsCsv() { return joinIds(clientIds()); }

QString clientLabel(const QString &id)
{
    if (id == QLatin1String("gemini")) return QStringLiteral("Gemini");
    for (const auto &c : kClients) if (c.id == id) return c.label;
    return id;
}

QString normalizeTrackedClientId(const QString &value)
{
    const auto id = value.trimmed().toLower();
    if (id == QLatin1String("kilocode")) return QStringLiteral("kilo");
    return id;
}

QString normalizeClientsCsv(const QString &value)
{
    QStringList out;
    QSet<QString> seen;
    for (const auto &part : value.split(QLatin1Char(','))) {
        const auto client = normalizeTrackedClientId(part);
        if (client.isEmpty() || seen.contains(client)) continue;
        seen.insert(client);
        out.append(client);
    }
    return joinIds(out);
}

QString clientsCsvForSetting(const QString &value)
{
    if (value.isNull()) return defaultClientsCsv();
    return normalizeClientsCsv(value);
}

QVector<LimitProviderInfo> limitProviderCatalog() { return kProviders; }

QStringList limitProviderIds()
{
    QStringList ids;
    for (const auto &p : kProviders) ids.append(p.id);
    return ids;
}

QString defaultLimitProvidersCsv() { return joinIds(limitProviderIds()); }

QString limitProviderLabel(const QString &id)
{
    for (const auto &p : kProviders) if (p.id == id) return p.label;
    return id;
}

QString limitProviderSettingsLabel(const QString &id)
{
    for (const auto &p : kProviders) if (p.id == id) return p.settingsLabel.isEmpty() ? p.label : p.settingsLabel;
    return id;
}

QString limitProviderForClient(const QString &clientId)
{
    if (clientId == QLatin1String("micode")) return QStringLiteral("mimo");
    if (clientId == QLatin1String("zcode")) return QStringLiteral("zai");
    if (clientId == QLatin1String("qodercn")) return QStringLiteral("qoder");
    return clientId;
}

QStringList tokscaleScanClientIds(const QString &clientId)
{
    if (clientId == QLatin1String("antigravity")) return {QStringLiteral("antigravity"), QStringLiteral("antigravity-cli")};
    if (clientId == QLatin1String("pi")) return {QStringLiteral("pi"), QStringLiteral("omp")};
    if (clientId == QLatin1String("kilo")) return {QStringLiteral("kilo"), QStringLiteral("kilocode")};
    return {clientId};
}

} // namespace tmon
