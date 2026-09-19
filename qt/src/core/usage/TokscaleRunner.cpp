#include "core/usage/TokscaleRunner.h"

#include "core/catalog/Catalog.h"
#include "core/process/Subprocess.h"
#include "core/tmon.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace tmon {
namespace {

QStringList filterClients(const QStringList &clients)
{
    QStringList out;
    const auto local = locallyParsedClientIds();
    for (const auto &id : clients) {
        if (local.contains(id)) continue;
        out.append(tokscaleScanClientIds(id));
    }
    out.removeDuplicates();
    return out;
}

TokscaleScan parseOutput(const SubprocessResult &run)
{
    TokscaleScan scan;
    if (!run.started) {
        scan.error = QStringLiteral("tokscale_missing");
        return scan;
    }
    if (run.timedOut) {
        scan.error = QStringLiteral("tokscale_timeout");
        return scan;
    }
    QJsonParseError err;
    const auto doc = QJsonDocument::fromJson(run.stdoutBytes, &err);
    if (err.error != QJsonParseError::NoError || doc.isNull()) {
        scan.error = QStringLiteral("tokscale_json: ") + QString::fromUtf8(run.stderrBytes.left(400));
        return scan;
    }
    scan.ok = true;
    scan.json = doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array());
    return scan;
}

} // namespace

TokscaleRunner::TokscaleRunner(QString binary)
    : TokscaleRunner(std::move(binary), QString())
{
}

TokscaleRunner::TokscaleRunner(QString binary, QString extraDirsEnv)
    : m_binary(std::move(binary))
    , m_extraDirsEnv(std::move(extraDirsEnv))
{
}

QMap<QString, QString> runnerEnv(const QString &extraDirsEnv)
{
    if (extraDirsEnv.isEmpty()) return {};
    return {{QStringLiteral("TOKSCALE_EXTRA_DIRS"), extraDirsEnv}};
}

TokscaleScan TokscaleRunner::scan(const QStringList &clients, const QStringList &extraArgs) const
{
    const auto csv = filterClients(clients).join(QLatin1Char(','));
    if (csv.isEmpty()) {
        TokscaleScan empty;
        empty.ok = true;
        empty.json = QJsonObject{};
        return empty;
    }
    const auto env = runnerEnv(m_extraDirsEnv);
    auto runOnce = [&](const QString &grouping) {
        QStringList args{
            QStringLiteral("--json"),
            QStringLiteral("--client"), csv,
            QStringLiteral("--group-by"), grouping
        };
        args.append(extraArgs);
        return runProcess(m_binary, args, kTokscaleTimeoutMs, {}, env);
    };

    auto result = parseOutput(runOnce(m_grouping));
    result.grouping = m_grouping;
    if (!result.ok && m_grouping.contains(QLatin1String("workspace"))) {
        m_grouping = QStringLiteral("client,session,model");
        result = parseOutput(runOnce(m_grouping));
        result.grouping = m_grouping;
    }
    return result;
}

TokscaleScan TokscaleRunner::scanToday(const QStringList &clients) const
{
    return scan(clients, {QStringLiteral("--today")});
}

TokscaleScan TokscaleRunner::scanMonth(const QStringList &clients) const
{
    return scan(clients, {QStringLiteral("--month")});
}

TokscaleScan TokscaleRunner::scanSince(const QStringList &clients, const QString &since) const
{
    return scan(clients, {QStringLiteral("--since"), since});
}

TokscaleScan TokscaleRunner::scanGraph(const QStringList &clients) const
{
    // Electron runTokscaleGraph: `tokscale graph --client <csv> --no-spinner`
    // (stdout is JSON; no --group-by and no grouping retry).
    const auto csv = filterClients(clients).join(QLatin1Char(','));
    if (csv.isEmpty()) {
        TokscaleScan empty;
        empty.ok = true;
        empty.json = QJsonObject{};
        return empty;
    }
    QStringList args{
        QStringLiteral("graph"),
        QStringLiteral("--client"), csv,
        QStringLiteral("--no-spinner")
    };
    const auto result = parseOutput(runProcess(m_binary, args, kTokscaleTimeoutMs, {},
                                               runnerEnv(m_extraDirsEnv)));
    return result;
}

} // namespace tmon
