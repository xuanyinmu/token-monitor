#include "core/catalog/ClientRoots.h"

#include "core/catalog/Catalog.h"
#include "core/io/Paths.h"

#include <QDir>
#include <QMap>
#include <QSet>
#include <QSysInfo>

namespace tmon {
namespace {

QString joinHome(const QString &rel)
{
    return QDir(Paths::homeDir()).filePath(rel);
}

void add(QVector<SourceRoot> &out, const QSet<QString> &enabled, const QString &client,
         const QString &checkId, const QString &dir, const QString &sourcePath = {},
         bool optional = false, bool custom = false)
{
    if (!enabled.contains(client)) return;
    SourceRoot root;
    root.client = client;
    root.checkId = checkId;
    root.dir = dir;
    root.sourcePath = sourcePath;
    root.optional = optional;
    root.custom = custom;
    out.append(root);
}

} // namespace

QVector<SourceRoot> clientSourceRoots(const QString &clientsCsv)
{
    QSet<QString> enabled;
    for (const auto &id : normalizeClientsCsv(clientsCsv).split(QLatin1Char(','), Qt::SkipEmptyParts))
        enabled.insert(id);

    QVector<SourceRoot> out;
    const QString home = Paths::homeDir();
    const QString xdg = Paths::xdgDataHome();
    const QString roaming = Paths::appDataRoaming();
    const QString local = Paths::appDataLocal();

    add(out, enabled, QStringLiteral("claude"), QStringLiteral("claude-projects"), joinHome(QStringLiteral(".claude/projects")));
    add(out, enabled, QStringLiteral("claude"), QStringLiteral("claude-transcripts"), joinHome(QStringLiteral(".claude/transcripts")));

    const QString codexHome = Paths::envOr(QStringLiteral("CODEX_HOME"), joinHome(QStringLiteral(".codex")));
    add(out, enabled, QStringLiteral("codex"), QStringLiteral("codex-sessions"), QDir(codexHome).filePath(QStringLiteral("sessions")));
    add(out, enabled, QStringLiteral("codex"), QStringLiteral("codex-sessions"), QDir(codexHome).filePath(QStringLiteral("archived_sessions")));

    add(out, enabled, QStringLiteral("hermes"), QStringLiteral("hermes-home"), Paths::envOr(QStringLiteral("HERMES_HOME"), joinHome(QStringLiteral(".hermes"))));
    add(out, enabled, QStringLiteral("opencode"), QStringLiteral("opencode-data"), QDir(xdg).filePath(QStringLiteral("opencode")));
    add(out, enabled, QStringLiteral("openclaw"), QStringLiteral("openclaw-agents"), joinHome(QStringLiteral(".openclaw/agents")));
    add(out, enabled, QStringLiteral("droid"), QStringLiteral("droid-sessions"), joinHome(QStringLiteral(".factory/sessions")));
    add(out, enabled, QStringLiteral("cursor"), QStringLiteral("tokscale-cursor-cache"),
        QDir(home).filePath(QStringLiteral(".config/tokscale/cursor-cache")));
    add(out, enabled, QStringLiteral("antigravity"), QStringLiteral("tokscale-antigravity-cache"),
        QDir(home).filePath(QStringLiteral(".config/antigravity-cache")));
    add(out, enabled, QStringLiteral("kimi"), QStringLiteral("kimi-sessions"), joinHome(QStringLiteral(".kimi/sessions")));
    add(out, enabled, QStringLiteral("kimi"), QStringLiteral("kimi-code-sessions"),
        Paths::envOr(QStringLiteral("KIMI_CODE_HOME"), joinHome(QStringLiteral(".kimi-code/sessions"))));
    add(out, enabled, QStringLiteral("qwen"), QStringLiteral("qwen-projects"), joinHome(QStringLiteral(".qwen/projects")));
    const QString grokHome = Paths::envOr(QStringLiteral("GROK_HOME"), joinHome(QStringLiteral(".grok")));
    add(out, enabled, QStringLiteral("grok"), QStringLiteral("grok-sessions"), QDir(grokHome).filePath(QStringLiteral("sessions")));
    add(out, enabled, QStringLiteral("grok"), QStringLiteral("grok-unified-log"), QDir(grokHome).filePath(QStringLiteral("logs")),
        QDir(grokHome).filePath(QStringLiteral("logs/unified.jsonl")));
    add(out, enabled, QStringLiteral("copilot"), QStringLiteral("copilot-data"), joinHome(QStringLiteral(".copilot")),
        joinHome(QStringLiteral(".copilot/data.db")));
    add(out, enabled, QStringLiteral("copilot"), QStringLiteral("vscode-workspace-storage"),
        QDir(roaming).filePath(QStringLiteral("Code/User/workspaceStorage")));
    add(out, enabled, QStringLiteral("pi"), QStringLiteral("pi-sessions"), joinHome(QStringLiteral(".pi/agent/sessions")));
    add(out, enabled, QStringLiteral("pi"), QStringLiteral("omp-sessions"), joinHome(QStringLiteral(".omp/agent/sessions")));
    add(out, enabled, QStringLiteral("zed"), QStringLiteral("zed-threads"), QDir(xdg).filePath(QStringLiteral("zed/threads")));
    add(out, enabled, QStringLiteral("zed"), QStringLiteral("zed-threads"), QDir(local).filePath(QStringLiteral("Zed/threads")));
    add(out, enabled, QStringLiteral("kilo"), QStringLiteral("kilo-db"), QDir(xdg).filePath(QStringLiteral("kilo")),
        QDir(xdg).filePath(QStringLiteral("kilo/kilo.db")));
    add(out, enabled, QStringLiteral("commandcode"), QStringLiteral("commandcode-projects"), joinHome(QStringLiteral(".commandcode/projects")));
    add(out, enabled, QStringLiteral("micode"), QStringLiteral("mimocode-data"), QDir(xdg).filePath(QStringLiteral("mimocode")));
    add(out, enabled, QStringLiteral("zcode"), QStringLiteral("zcode-projects"), joinHome(QStringLiteral(".zcode/projects")));
    add(out, enabled, QStringLiteral("zcode"), QStringLiteral("zcode-cli-db"), joinHome(QStringLiteral(".zcode/cli/db")),
        joinHome(QStringLiteral(".zcode/cli/db/db.sqlite")));
    add(out, enabled, QStringLiteral("codebuddy"), QStringLiteral("codebuddy-projects"), joinHome(QStringLiteral(".codebuddy/projects")));
    add(out, enabled, QStringLiteral("codebuddy"), QStringLiteral("codebuddy-extension-logs"),
        QDir(local).filePath(QStringLiteral("CodeBuddyExtension/Logs")));
    add(out, enabled, QStringLiteral("workbuddy"), QStringLiteral("workbuddy-projects"), joinHome(QStringLiteral(".workbuddy/projects")));
    add(out, enabled, QStringLiteral("proma"), QStringLiteral("proma-sessions"), joinHome(QStringLiteral(".proma/agent-sessions")));
    add(out, enabled, QStringLiteral("qodercn"), QStringLiteral("qodercn-db"),
        QDir(roaming).filePath(QStringLiteral("QoderCN/SharedClientCache/cache/db")),
        QDir(roaming).filePath(QStringLiteral("QoderCN/SharedClientCache/cache/db/local.db")));
    add(out, enabled, QStringLiteral("reasonix"), QStringLiteral("reasonix-stats"),
        Paths::envOr(QStringLiteral("REASONIX_STATE_HOME"), joinHome(QStringLiteral(".reasonix/stats"))));
    add(out, enabled, QStringLiteral("dsh"), QStringLiteral("dsh-sessions"),
        QDir(Paths::envOr(QStringLiteral("DSH_HOME"), joinHome(QStringLiteral(".dsh")))).filePath(QStringLiteral("sessions")));
    add(out, enabled, QStringLiteral("kiro"), QStringLiteral("kiro-sessions"), joinHome(QStringLiteral(".kiro/sessions")));
    add(out, enabled, QStringLiteral("kiro"), QStringLiteral("kiro-ide-globalstorage"),
        QDir(roaming).filePath(QStringLiteral("Kiro/User/globalStorage/kiro.kiroagent")));
    add(out, enabled, QStringLiteral("kiro"), QStringLiteral("kiro-cli-data"), joinHome(QStringLiteral(".local/share/kiro-cli")));
    add(out, enabled, QStringLiteral("cline"), QStringLiteral("cline-tasks"),
        QDir(roaming).filePath(QStringLiteral("Code/User/globalStorage/saoudrizwan.claude-dev/tasks")));
    add(out, enabled, QStringLiteral("cline"), QStringLiteral("cline-cli-sessions"), joinHome(QStringLiteral(".cline/data/sessions")));
    add(out, enabled, QStringLiteral("cherrystudio"), QStringLiteral("cherrystudio-v2"),
        QDir(roaming).filePath(QStringLiteral("CherryStudio/Data/Agents/.claude/projects")));
    add(out, enabled, QStringLiteral("cherrystudio"), QStringLiteral("cherrystudio-legacy"),
        QDir(roaming).filePath(QStringLiteral("CherryStudio/.claude/projects")));
    add(out, enabled, QStringLiteral("lmstudio"), QStringLiteral("lmstudio-server-logs"),
        QDir(Paths::envOr(QStringLiteral("LM_STUDIO_HOME"), joinHome(QStringLiteral(".lmstudio")))).filePath(QStringLiteral("server-logs")));
    add(out, enabled, QStringLiteral("unsloth"), QStringLiteral("unsloth-db"),
        Paths::envOr(QStringLiteral("UNSLOTH_STUDIO_HOME"), joinHome(QStringLiteral(".unsloth/studio"))),
        QDir(Paths::envOr(QStringLiteral("UNSLOTH_STUDIO_HOME"), joinHome(QStringLiteral(".unsloth/studio")))).filePath(QStringLiteral("studio.db")));
    add(out, enabled, QStringLiteral("antigravity"), QStringLiteral("antigravity-cli"),
        joinHome(QStringLiteral(".gemini/antigravity-cli/conversations")));
    return out;
}

QStringList watchDirsForClients(const QString &clientsCsv)
{
    QStringList dirs;
    QSet<QString> seen;
    const QSet<QString> skipCache{QStringLiteral("tokscale-cursor-cache"), QStringLiteral("tokscale-antigravity-cache")};
    const QSet<QString> intervalOnly{QStringLiteral("kiro-ide-globalstorage")};
    for (const auto &root : clientSourceRoots(clientsCsv)) {
        if (skipCache.contains(root.checkId) || intervalOnly.contains(root.checkId)) continue;
        if (root.dir.isEmpty() || seen.contains(root.dir)) continue;
        seen.insert(root.dir);
        dirs.append(root.dir);
    }
    return dirs;
}

QStringList selfSyncedClients()
{
    return {QStringLiteral("cursor"), QStringLiteral("antigravity")};
}

QStringList wslDataMarkers()
{
    return {
        QStringLiteral(".claude/projects"), QStringLiteral(".claude/transcripts"),
        QStringLiteral(".codex/sessions"), QStringLiteral(".local/share/opencode"),
        QStringLiteral(".openclaw/agents"), QStringLiteral(".hermes"),
        QStringLiteral(".kimi/sessions"), QStringLiteral(".kimi-code/sessions"),
        QStringLiteral(".qwen/projects"), QStringLiteral(".grok/sessions"),
        QStringLiteral(".copilot/otel"), QStringLiteral(".gemini/antigravity-cli/conversations"),
        QStringLiteral(".pi/agent/sessions"), QStringLiteral(".dsh/sessions"),
        QStringLiteral(".factory/sessions"), QStringLiteral(".zcode/projects"),
        QStringLiteral(".kiro/sessions"), QStringLiteral(".codebuddy/projects"),
        QStringLiteral(".workbuddy"), QStringLiteral(".proma/agent-sessions"),
        QStringLiteral(".lmstudio/server-logs"), QStringLiteral(".unsloth/studio/studio.db")
    };
}

QString wslMarkerClient(const QString &marker)
{
    static const QMap<QString, QString> map{
        {QStringLiteral(".claude/projects"), QStringLiteral("claude")},
        {QStringLiteral(".claude/transcripts"), QStringLiteral("claude")},
        {QStringLiteral(".codex/sessions"), QStringLiteral("codex")},
        {QStringLiteral(".local/share/opencode"), QStringLiteral("opencode")},
        {QStringLiteral(".openclaw/agents"), QStringLiteral("openclaw")},
        {QStringLiteral(".hermes"), QStringLiteral("hermes")},
        {QStringLiteral(".kimi/sessions"), QStringLiteral("kimi")},
        {QStringLiteral(".kimi-code/sessions"), QStringLiteral("kimi")},
        {QStringLiteral(".qwen/projects"), QStringLiteral("qwen")},
        {QStringLiteral(".grok/sessions"), QStringLiteral("grok")},
        {QStringLiteral(".copilot/otel"), QStringLiteral("copilot")},
        {QStringLiteral(".gemini/antigravity-cli/conversations"), QStringLiteral("antigravity")},
        {QStringLiteral(".pi/agent/sessions"), QStringLiteral("pi")},
        {QStringLiteral(".dsh/sessions"), QStringLiteral("dsh")},
        {QStringLiteral(".factory/sessions"), QStringLiteral("droid")},
        {QStringLiteral(".zcode/projects"), QStringLiteral("zcode")},
        {QStringLiteral(".kiro/sessions"), QStringLiteral("kiro")},
        {QStringLiteral(".codebuddy/projects"), QStringLiteral("codebuddy")},
        {QStringLiteral(".workbuddy"), QStringLiteral("workbuddy")},
        {QStringLiteral(".proma/agent-sessions"), QStringLiteral("proma")},
        {QStringLiteral(".lmstudio/server-logs"), QStringLiteral("lmstudio")},
        {QStringLiteral(".unsloth/studio/studio.db"), QStringLiteral("unsloth")}
    };
    return map.value(marker);
}

} // namespace tmon
