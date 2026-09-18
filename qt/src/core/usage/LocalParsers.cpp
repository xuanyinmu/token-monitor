#include "core/usage/LocalParsers.h"

#include "core/catalog/ClientRoots.h"
#include "core/io/JsonIo.h"
#include "core/io/Paths.h"
#include "core/usage/JsonUtil.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTime>
#include <QTimeZone>
#include <cmath>

namespace tmon {
namespace {

QJsonObject parseProma(const QJsonObject &period)
{
    auto out = period;
    const QString dir = QDir(Paths::homeDir()).filePath(QStringLiteral(".proma/agent-sessions"));
    QDir d(dir);
    if (!d.exists()) return out;
    double tokens = asNumber(out.value(QStringLiteral("clients")).toObject().value(QStringLiteral("proma")));
    for (const auto &info : d.entryInfoList({QStringLiteral("*.jsonl")}, QDir::Files)) {
        QFile file(info.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly)) continue;
        while (!file.atEnd()) {
            const auto line = file.readLine();
            const auto doc = QJsonDocument::fromJson(line);
            if (!doc.isObject()) continue;
            tokens += tokenValue(doc.object());
        }
    }
    if (tokens > 0) {
        auto clients = out.value(QStringLiteral("clients")).toObject();
        clients.insert(QStringLiteral("proma"), tokens);
        out.insert(QStringLiteral("clients"), clients);
        const double current = asNumber(out.value(QStringLiteral("totalTokens")));
        const double previous = asNumber(period.value(QStringLiteral("clients")).toObject().value(QStringLiteral("proma")));
        out.insert(QStringLiteral("totalTokens"), current - previous + tokens);
    }
    return out;
}

qint64 qoderTimestampMs(const QVariant &value)
{
    if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::LongLong || value.typeId() == QMetaType::Int) {
        const double n = value.toDouble();
        if (!std::isfinite(n) || n <= 0) return 0;
        return n < 1'000'000'000'000 ? qint64(n * 1000) : qint64(n);
    }
    const auto text = value.toString().trimmed();
    if (text.isEmpty()) return 0;
    bool ok = false;
    const double n = text.toDouble(&ok);
    if (ok && n > 0) return n < 1'000'000'000'000 ? qint64(n * 1000) : qint64(n);
    const auto dt = QDateTime::fromString(text, Qt::ISODate);
    return dt.isValid() ? dt.toMSecsSinceEpoch() : 0;
}

QJsonObject parseQoderCn(const QJsonObject &period, const QString &window)
{
    auto out = period;
    const QString dbPath = Paths::envOr(
        QStringLiteral("TOKEN_MONITOR_QODER_CN_DB_PATH"),
        QDir(Paths::appDataRoaming()).filePath(QStringLiteral("QoderCN/SharedClientCache/cache/db/local.db")));
    if (!QFileInfo::exists(dbPath)) return out;
    const auto conn = QStringLiteral("tmon-qodercn");
    auto db = QSqlDatabase::contains(conn) ? QSqlDatabase::database(conn) : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), conn);
    db.setDatabaseName(dbPath);
    if (!db.open()) return out;

    bool hasProject = false;
    {
        QSqlQuery probe(db);
        if (probe.exec(QStringLiteral(
                "SELECT 1 FROM sqlite_master WHERE type='table' AND name='chat_session' "
                "AND EXISTS (SELECT 1 FROM pragma_table_info('chat_session') WHERE name='project_name') LIMIT 1"))) {
            hasProject = probe.next();
        }
    }

    QSqlQuery q(db);
    const auto sql = hasProject
        ? QStringLiteral("SELECT rowid AS row_id, id, session_id, request_id, token_info, model_info, gmt_create, "
                         "(SELECT cs.project_name FROM chat_session cs WHERE cs.session_id = chat_message.session_id LIMIT 1) AS project_name "
                         "FROM chat_message WHERE role='assistant' AND token_info IS NOT NULL AND trim(token_info) NOT IN ('','{}') "
                         "ORDER BY gmt_create, rowid")
        : QStringLiteral("SELECT rowid AS row_id, id, session_id, request_id, token_info, model_info, gmt_create "
                         "FROM chat_message WHERE role='assistant' AND token_info IS NOT NULL AND trim(token_info) NOT IN ('','{}') "
                         "ORDER BY gmt_create, rowid");
    if (!q.exec(sql)) return out;

    qint64 cutoff = 0;
    const auto now = QDateTime::currentDateTime();
    if (window == QLatin1String("today"))
        cutoff = QDateTime(now.date(), QTime(0, 0), now.timeZone()).toMSecsSinceEpoch();
    else if (window == QLatin1String("month"))
        cutoff = QDateTime(QDate(now.date().year(), now.date().month(), 1), QTime(0, 0), now.timeZone()).toMSecsSinceEpoch();

    double tokens = 0;
    int rows = 0;
    auto models = out.value(QStringLiteral("models")).toObject();
    auto clients = out.value(QStringLiteral("clients")).toObject();
    const double previous = asNumber(clients.value(QStringLiteral("qodercn")));
    while (q.next() && rows < 100000) {
        ++rows;
        const auto at = qoderTimestampMs(q.value(QStringLiteral("gmt_create")));
        if (cutoff > 0 && at > 0 && at < cutoff) continue;
        const auto usage = QJsonDocument::fromJson(q.value(QStringLiteral("token_info")).toByteArray()).object();
        const double prompt = asNumber(usage.value(QStringLiteral("prompt_tokens")));
        const double output = asNumber(usage.value(QStringLiteral("completion_tokens")));
        if (prompt + output <= 0) continue;
        tokens += prompt + output;
        const auto modelInfo = QJsonDocument::fromJson(q.value(QStringLiteral("model_info")).toByteArray()).object();
        auto model = modelInfo.value(QStringLiteral("model_key")).toString(modelInfo.value(QStringLiteral("modelKey")).toString());
        if (model.isEmpty()) model = QStringLiteral("qoder-agent");
        models.insert(model, asNumber(models.value(model)) + prompt + output);
    }
    if (tokens > 0) {
        clients.insert(QStringLiteral("qodercn"), tokens);
        out.insert(QStringLiteral("clients"), clients);
        out.insert(QStringLiteral("models"), models);
        out.insert(QStringLiteral("totalTokens"), asNumber(out.value(QStringLiteral("totalTokens"))) - previous + tokens);
    }
    return out;
}

} // namespace

QJsonObject mergeLocalParsers(const QJsonObject &period, const QStringList &clients, const QString &window)
{
    auto out = period;
    if (clients.contains(QStringLiteral("proma"))) out = parseProma(out);
    if (clients.contains(QStringLiteral("qodercn"))) out = parseQoderCn(out, window);
    return out;
}

} // namespace tmon
