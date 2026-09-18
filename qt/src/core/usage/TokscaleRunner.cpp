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
    : m_binary(std::move(binary))
{
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
    auto runOnce = [&](const QString &grouping) {
        QStringList args{
            QStringLiteral("--json"),
            QStringLiteral("--client"), csv,
            QStringLiteral("--group-by"), grouping
        };
        args.append(extraArgs);
        return runProcess(m_binary, args, kTokscaleTimeoutMs);
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

} // namespace tmon
