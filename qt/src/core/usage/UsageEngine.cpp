#include "core/usage/UsageEngine.h"

#include "core/catalog/Catalog.h"
#include "core/catalog/ClientRoots.h"
#include "core/io/JsonIo.h"
#include "core/io/Paths.h"
#include "core/tmon.h"
#include "core/usage/JsonUtil.h"
#include "core/usage/LocalParsers.h"
#include "core/usage/PeriodDelta.h"
#include "core/usage/SelfSync.h"
#include "core/usage/UsageNormalize.h"
#include "core/usage/WslUsage.h"

#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHostInfo>
#include <QJsonArray>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <cmath>

namespace tmon {

namespace {

double graphNumber(const QJsonObject &obj, const char *camel, const char *snake)
{
    double v = obj.value(QLatin1String(camel)).toDouble();
    if (v == 0 && snake) v = obj.value(QLatin1String(snake)).toDouble();
    return v;
}

// history.js sumTokens: input + output + cacheRead + cacheWrite, with
// reasoning as a disjoint bucket for the reasonix/codex/droid/dsh clients.
double graphSumTokens(const QJsonObject &tokens, const QString &client)
{
    double total = graphNumber(tokens, "input", nullptr)
        + graphNumber(tokens, "output", nullptr)
        + graphNumber(tokens, "cacheRead", "cache_read")
        + graphNumber(tokens, "cacheWrite", "cache_write");
    static const QSet<QString> kDisjointReasoning{
        QStringLiteral("reasonix"), QStringLiteral("codex"),
        QStringLiteral("droid"), QStringLiteral("dsh")
    };
    if (kDisjointReasoning.contains(client))
        total += tokens.value(QStringLiteral("reasoning")).toDouble();
    return total;
}

// history.js normalizeTokscaleClientName aliases.
QString normalizeTokscaleAlias(const QString &client)
{
    if (client == QLatin1String("omp")) return QStringLiteral("pi");
    if (client == QLatin1String("kilocode")) return QStringLiteral("kilo");
    return client;
}

// parseGraphResult port: fold tokscale graph contributions into per-day
// records {tokens, costUsd, activeTimeMs, clients, clientCosts, models,
// modelCosts}, capped to the newest 370 days like normalizeHistory's rolling
// daily window.
QJsonObject graphHistoryDays(const QJsonObject &graph)
{
    QJsonObject days;
    const auto contributions = graph.value(QStringLiteral("contributions")).toArray();
    for (const auto &rowV : contributions) {
        const auto row = rowV.toObject();
        const auto date = row.value(QStringLiteral("date")).toString().left(10);
        if (date.isEmpty()) continue;
        double tokens = 0;
        double cost = 0;
        QJsonObject clients, clientCosts, models, modelCosts;
        const auto clientRows = row.value(QStringLiteral("clients")).toArray();
        for (const auto &cV : clientRows) {
            const auto c = cV.toObject();
            const auto client = normalizeTokscaleAlias(c.value(QStringLiteral("client")).toString().trimmed().toLower());
            const auto model = c.value(QStringLiteral("modelId")).toString(
                c.value(QStringLiteral("model")).toString(c.value(QStringLiteral("model_id")).toString()));
            const double t = graphSumTokens(c.value(QStringLiteral("tokens")).toObject(), client);
            const double cst = c.value(QStringLiteral("cost")).toDouble();
            tokens += t;
            cost += cst;
            if (!client.isEmpty()) {
                clients.insert(client, clients.value(client).toDouble() + t);
                clientCosts.insert(client, clientCosts.value(client).toDouble() + cst);
            }
            if (!model.isEmpty()) {
                models.insert(model, models.value(model).toDouble() + t);
                modelCosts.insert(model, modelCosts.value(model).toDouble() + cst);
            }
        }
        QJsonObject day{
            {QStringLiteral("tokens"), qMax(0.0, std::round(tokens))},
            {QStringLiteral("costUsd"), cost}
        };
        const double activeMs = graphNumber(row, "activeTimeMs", "active_time_ms");
        if (activeMs > 0) day.insert(QStringLiteral("activeTimeMs"), activeMs);
        if (!clients.isEmpty()) day.insert(QStringLiteral("clients"), clients);
        if (!clientCosts.isEmpty()) day.insert(QStringLiteral("clientCosts"), clientCosts);
        if (!models.isEmpty()) day.insert(QStringLiteral("models"), models);
        if (!modelCosts.isEmpty()) day.insert(QStringLiteral("modelCosts"), modelCosts);
        days.insert(date, day);
    }
    const auto keys = days.keys();
    const int cutoff = keys.size() > 370 ? keys.size() - 370 : 0;
    for (int i = 0; i < cutoff; ++i)
        days.remove(keys[i]);
    return days;
}

} // namespace

UsageEngine::UsageEngine(QObject *parent)
    : QObject(parent)
    , m_runner(Paths::tokscaleBinary())
    , m_watcher(new UsageWatcher(this))
    , m_interval(new QTimer(this))
{
    m_snapshot = QJsonObject{
        {QStringLiteral("today"), emptyPeriod()},
        {QStringLiteral("month"), emptyPeriod()},
        {QStringLiteral("allTime"), emptyPeriod()}
    };
    m_history = readJsonObject(Paths::historyPath());
    if (m_history.isEmpty()) m_history = QJsonObject{{QStringLiteral("days"), QJsonObject{}}};
    connect(m_watcher, &UsageWatcher::dirty, this, [this](const QStringList &) { requestScan(false); });
    connect(m_interval, &QTimer::timeout, this, [this]() { requestScan(true); });
}

void UsageEngine::configure(const QJsonObject &settings)
{
    m_settings = settings;
    m_runner = TokscaleRunner(Paths::tokscaleBinary(),
                              tokscaleExtraDirsEnv(settings.value(QStringLiteral("customScanPaths")).toObject()));
    // Electron's collector fingerprint restart clears lastHistoryAt, so a
    // tracked-clients change re-runs the graph with the new filter.
    const auto clientsCsv = settings.value(QStringLiteral("clients")).toString(defaultClientsCsv());
    if (clientsCsv != m_graphClients) {
        m_lastGraphAt = 0;
        m_graphClients = clientsCsv;
    }
    m_interval->setInterval(qMax(30'000, settings.value(QStringLiteral("collectionIntervalMs")).toInt(kDefaultCollectionIntervalMs)));
    m_watcher->setRoots(watchDirsForClients(settings.value(QStringLiteral("clients")).toString(defaultClientsCsv())));
    hydrateFromDisk();
}

void UsageEngine::hydrateFromDisk()
{
    m_history = readJsonObject(Paths::historyPath());
    if (m_history.isEmpty())
        m_history = QJsonObject{{QStringLiteral("days"), QJsonObject{}}};
    QJsonObject archive = readJsonObject(QDir(Paths::userDataDir()).filePath(QStringLiteral("daily-history-archive.json")));
    if (archive.isEmpty())
        archive = readJsonObject(QDir(Paths::sharedDataDir()).filePath(QStringLiteral("daily-history-archive.json")));
    if (archive.contains(QStringLiteral("days"))) {
        auto days = m_history.value(QStringLiteral("days")).toObject();
        const auto src = archive.value(QStringLiteral("days")).toObject();
        for (auto it = src.begin(); it != src.end(); ++it) {
            const auto day = it.value().toObject();
            double tokens = 0;
            double cost = 0;
            if (day.contains(QStringLiteral("tokens")) || day.contains(QStringLiteral("totalTokens")))
                tokens = day.value(QStringLiteral("tokens")).toDouble(day.value(QStringLiteral("totalTokens")).toDouble());
            if (day.contains(QStringLiteral("costUsd")) || day.contains(QStringLiteral("cost")))
                cost = day.value(QStringLiteral("costUsd")).toDouble(day.value(QStringLiteral("cost")).toDouble());
            const auto observations = day.value(QStringLiteral("observations")).toObject();
            QJsonObject clients, clientCosts, models, modelCosts;
            if (!observations.isEmpty()) {
                // Electron archive observations are keyed "JSON([client, modelId])".
                // Fold them into per-client/model token+cost maps so fixed-range
                // derivations (本周/最近 7 天/最近 30 天) keep tool attribution.
                for (auto obs = observations.begin(); obs != observations.end(); ++obs) {
                    const auto row = obs.value().toObject();
                    const double rowTokens = row.value(QStringLiteral("tokens")).toDouble();
                    const double rowCost = row.value(QStringLiteral("cost")).toDouble(row.value(QStringLiteral("costUsd")).toDouble());
                    if (rowTokens == 0 && rowCost == 0) continue;
                    const auto client = row.value(QStringLiteral("client")).toString();
                    const auto model = row.value(QStringLiteral("modelId")).toString(row.value(QStringLiteral("model")).toString());
                    if (!client.isEmpty()) {
                        clients.insert(client, clients.value(client).toDouble() + rowTokens);
                        if (rowCost != 0)
                            clientCosts.insert(client, clientCosts.value(client).toDouble() + rowCost);
                    }
                    if (!model.isEmpty()) {
                        models.insert(model, models.value(model).toDouble() + rowTokens);
                        if (rowCost != 0)
                            modelCosts.insert(model, modelCosts.value(model).toDouble() + rowCost);
                    }
                }
                if (tokens == 0 && cost == 0) {
                    for (auto ct = clients.begin(); ct != clients.end(); ++ct) tokens += ct.value().toDouble();
                    for (auto cc = clientCosts.begin(); cc != clientCosts.end(); ++cc) cost += cc.value().toDouble();
                }
            }
            const auto archiveTime = day.value(QStringLiteral("activeTimeMs"));
            if (days.contains(it.key())) {
                auto existing = days.value(it.key()).toObject();
                if (!existing.contains(QStringLiteral("activeTimeMs")) && archiveTime.isDouble())
                    existing.insert(QStringLiteral("activeTimeMs"), archiveTime);
                // Attribution maps stay absent when the record already has them
                // (Qt's scan-authored today) so archive rows never double-count.
                if (!existing.contains(QStringLiteral("clients")) && !clients.isEmpty())
                    existing.insert(QStringLiteral("clients"), clients);
                if (!existing.contains(QStringLiteral("clientCosts")) && !clientCosts.isEmpty())
                    existing.insert(QStringLiteral("clientCosts"), clientCosts);
                if (!existing.contains(QStringLiteral("models")) && !models.isEmpty())
                    existing.insert(QStringLiteral("models"), models);
                if (!existing.contains(QStringLiteral("modelCosts")) && !modelCosts.isEmpty())
                    existing.insert(QStringLiteral("modelCosts"), modelCosts);
                days.insert(it.key(), existing);
                continue;
            }
            QJsonObject rec{
                {QStringLiteral("tokens"), tokens},
                {QStringLiteral("costUsd"), cost}
            };
            if (!clients.isEmpty()) rec.insert(QStringLiteral("clients"), clients);
            if (!clientCosts.isEmpty()) rec.insert(QStringLiteral("clientCosts"), clientCosts);
            if (!models.isEmpty()) rec.insert(QStringLiteral("models"), models);
            if (!modelCosts.isEmpty()) rec.insert(QStringLiteral("modelCosts"), modelCosts);
            if (archiveTime.isDouble())
                rec.insert(QStringLiteral("activeTimeMs"), archiveTime);
            days.insert(it.key(), rec);
        }
        m_history.insert(QStringLiteral("days"), days);
    }
    const auto anchor = readJsonObject(Paths::collectorAnchorPath());
    if (anchor.isEmpty()) return;
    m_today = anchor.value(QStringLiteral("today")).toObject();
    m_month = anchor.value(QStringLiteral("month")).toObject();
    m_allTime = anchor.value(QStringLiteral("allTime")).toObject();
    m_anchorToday = m_today;
    m_anchorDate = QDate::fromString(anchor.value(QStringLiteral("date")).toString(), Qt::ISODate);
    if (!m_anchorDate.isValid()) m_anchorDate = QDate::currentDate();
    m_snapshot.insert(QStringLiteral("today"), m_today);
    m_snapshot.insert(QStringLiteral("month"), m_month);
    m_snapshot.insert(QStringLiteral("allTime"), m_allTime);
    m_snapshot.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    m_snapshot.insert(QStringLiteral("periodWindows"), periodWindowsNow());
    m_snapshot.insert(QStringLiteral("trackedClients"), QJsonArray::fromStringList(trackedClients()));
    m_snapshot.insert(QStringLiteral("deviceId"), m_settings.value(QStringLiteral("deviceId")));
    m_snapshot.insert(QStringLiteral("hostname"), QHostInfo::localHostName());
    m_snapshot.insert(QStringLiteral("platform"), QSysInfo::kernelType() + QLatin1Char('-') + QSysInfo::currentCpuArchitecture());
    m_snapshot.insert(QStringLiteral("osName"), QSysInfo::prettyProductName());
    m_snapshot.insert(QStringLiteral("agentVersion"), QString::fromUtf8(kAppVersion));
    m_snapshot.insert(QStringLiteral("history"), m_history);
    emit statusChanged(QStringLiteral("Live"));
    emit updated();
}

QStringList UsageEngine::trackedClients() const
{
    return normalizeClientsCsv(m_settings.value(QStringLiteral("clients")).toString(defaultClientsCsv()))
        .split(QLatin1Char(','), Qt::SkipEmptyParts);
}

void UsageEngine::start()
{
    m_running = true;
    emit statusChanged(QStringLiteral("Scanning"));
    // Electron collection modes: 'live' watches + ticks on the configured
    // cadence, 'smart' watches and backs off to a fixed 10m tick, 'interval'
    // drops the watcher and runs on the cadence alone.
    const auto mode = m_settings.value(QStringLiteral("collectionMode")).toString(QStringLiteral("live"));
    if (mode != QLatin1String("interval"))
        m_watcher->setRoots(watchDirsForClients(m_settings.value(QStringLiteral("clients")).toString(defaultClientsCsv())));
    // Hydrate-from-disk already painted a first frame. Defer tokscale so the
    // event loop can process limits HTTP and chrome instead of freezing the widget.
    QTimer::singleShot(0, this, [this]() {
        if (!m_running) return;
        requestScan(true);
        const auto mode = m_settings.value(QStringLiteral("collectionMode")).toString(QStringLiteral("live"));
        if (m_running && mode != QLatin1String("manual")) {
            m_interval->setInterval(mode == QLatin1String("smart")
                ? 10 * 60 * 1000
                : qMax(30'000, m_settings.value(QStringLiteral("collectionIntervalMs")).toInt(kDefaultCollectionIntervalMs)));
            m_interval->start();
        }
    });
}

void UsageEngine::stop()
{
    ++m_generation;
    m_running = false;
    m_pendingFull = false;
    m_pendingToday = false;
    m_interval->stop();
    m_watcher->stop();
}

bool UsageEngine::scanOnce()
{
    configure(m_settings.isEmpty() ? QJsonObject{{QStringLiteral("clients"), defaultClientsCsv()}} : m_settings);
    return fullScan();
}

void UsageEngine::requestScan(bool full)
{
    if (m_scanBusy) {
        if (full) m_pendingFull = true;
        else m_pendingToday = true;
        return;
    }
    beginScan(full);
}

void UsageEngine::beginScan(bool full)
{
    if (!full && (m_anchorDate != QDate::currentDate() || m_anchorToday.isEmpty()))
        full = true;
    m_scanBusy = true;
    const qint64 gen = ++m_generation;
    const auto clients = trackedClients();
    const auto settings = m_settings;
    const auto binary = m_runner.binary();
    const auto extraDirs = tokscaleExtraDirsEnv(settings.value(QStringLiteral("customScanPaths")).toObject());
    // Electron shouldIncludeHistory: a gated graph run per historyIntervalMs
    // (forced on a local-day rollover — the anchor-date full scan here).
    const bool historyOn = settings.value(QStringLiteral("historyEnabled")).toBool(true);
    const qint64 graphInterval = qMax(300'000, settings.value(QStringLiteral("historyIntervalMs")).toInt(900'000));
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const bool dayRolled = m_anchorDate != QDate::currentDate();
    const bool wantGraph = full && historyOn
        && (m_lastGraphAt == 0 || nowMs - m_lastGraphAt >= graphInterval || dayRolled);
    emit statusChanged(QStringLiteral("Scanning"));
    QPointer<UsageEngine> self(this);
    auto *thread = QThread::create([self, full, gen, clients, settings, binary, extraDirs, wantGraph]() {
        TokscaleRunner runner(binary, extraDirs);
        if (!full)
            maybeSelfSync(clients, binary, true);
        TokscaleScan today = runner.scanToday(clients);
        if (self) {
            QMetaObject::invokeMethod(self.data(), [self, gen, clients, today]() {
                if (!self || gen != self->m_generation) return;
                self->applyTodayPartial(gen, clients, today);
            }, Qt::QueuedConnection);
        }
        TokscaleScan month;
        TokscaleScan allTime;
        QJsonObject wsl;
        TokscaleScan graph;
        if (full) {
            maybeSelfSync(clients, binary, false);
            month = runner.scanMonth(clients);
            const auto since = settings.value(QStringLiteral("allTimeSince")).toString(QStringLiteral("2024-01-01"));
            allTime = runner.scanSince(clients, since);
            if (settings.value(QStringLiteral("wslScanEnabled")).toBool(true))
                wsl = scanWslUsage(clients, since);
            if (wantGraph)
                graph = runner.scanGraph(clients);
        }
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, full, gen, clients, today, month, allTime, wsl, graph, wantGraph]() {
            if (!self) return;
            self->applyWorkerScan(full, gen, clients, today, month, allTime, wsl, graph);
            if (wantGraph && graph.ok)
                self->m_lastGraphAt = QDateTime::currentMSecsSinceEpoch();
        }, Qt::QueuedConnection);
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void UsageEngine::applyTodayPartial(qint64 gen, const QStringList &clients, TokscaleScan today)
{
    if (gen != m_generation || !today.ok) return;
    const bool resolveProjects = m_settings.value(QStringLiteral("projectsEnabled")).toBool(true);
    auto fresh = extractUsageFromTokscale(today.json, resolveProjects);
    fresh = mergeLocalParsers(fresh, clients, QStringLiteral("today"));
    if (!m_anchorToday.isEmpty() && m_anchorDate == QDate::currentDate()) {
        m_month = applyPeriodDelta(m_month, fresh, m_anchorToday).toObject();
        m_allTime = applyPeriodDelta(m_allTime, fresh, m_anchorToday).toObject();
    }
    m_today = fresh;
    m_snapshot.insert(QStringLiteral("today"), m_today);
    m_snapshot.insert(QStringLiteral("month"), m_month);
    m_snapshot.insert(QStringLiteral("allTime"), m_allTime);
    m_snapshot.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    emit updated();
}

void UsageEngine::applyWorkerScan(bool full, qint64 gen, const QStringList &clients,
                                 TokscaleScan today, TokscaleScan month, TokscaleScan allTime,
                                 QJsonObject wsl, TokscaleScan graph)
{
    const bool stale = gen != m_generation;
    auto finish = [this]() {
        m_scanBusy = false;
        if (!m_running) {
            m_pendingFull = false;
            m_pendingToday = false;
            return;
        }
        if (m_pendingFull) {
            m_pendingFull = false;
            m_pendingToday = false;
            beginScan(true);
        } else if (m_pendingToday) {
            m_pendingToday = false;
            beginScan(false);
        }
    };
    if (stale) {
        finish();
        return;
    }
    if (full) {
        if (!today.ok) {
            m_lastError = today.error;
            emit statusChanged(m_lastError);
            finish();
            return;
        }
        const bool resolveProjects = m_settings.value(QStringLiteral("projectsEnabled")).toBool(true);
        m_today = extractUsageFromTokscale(today.json, resolveProjects);
        m_month = month.ok ? extractUsageFromTokscale(month.json, resolveProjects) : m_today;
        m_allTime = allTime.ok ? extractUsageFromTokscale(allTime.json, resolveProjects) : m_month;
        m_today = mergeLocalParsers(m_today, clients, QStringLiteral("today"));
        m_month = mergeLocalParsers(m_month, clients, QStringLiteral("month"));
        m_allTime = mergeLocalParsers(m_allTime, clients, QStringLiteral("allTime"));
        if (!wsl.isEmpty()) {
            m_today = mergePeriods(m_today, wsl.value(QStringLiteral("today")).toObject());
            m_month = mergePeriods(m_month, wsl.value(QStringLiteral("month")).toObject());
            m_allTime = mergePeriods(m_allTime, wsl.value(QStringLiteral("allTime")).toObject());
            m_snapshot.insert(QStringLiteral("wslStatus"), wsl.value(QStringLiteral("status")));
        }
        m_anchorToday = m_today;
        m_anchorDate = QDate::currentDate();
        persistAnchor();
        // The graph history lands before today's scan record so the live
        // snapshot keeps the freshest today row (its activeTimeMs rides along).
        if (graph.ok)
            applyGraphHistory(graph.json.toObject());
        mergeHistoryDay(m_today);
        persistHistory();
        m_snapshot.insert(QStringLiteral("today"), m_today);
        m_snapshot.insert(QStringLiteral("month"), m_month);
        m_snapshot.insert(QStringLiteral("allTime"), m_allTime);
        m_snapshot.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        m_snapshot.insert(QStringLiteral("periodWindows"), periodWindowsNow());
        m_snapshot.insert(QStringLiteral("trackedClients"), QJsonArray::fromStringList(clients));
        m_snapshot.insert(QStringLiteral("deviceId"), m_settings.value(QStringLiteral("deviceId")));
        m_snapshot.insert(QStringLiteral("hostname"), QHostInfo::localHostName());
        m_snapshot.insert(QStringLiteral("platform"), QSysInfo::kernelType() + QLatin1Char('-') + QSysInfo::currentCpuArchitecture());
        m_snapshot.insert(QStringLiteral("osName"), QSysInfo::prettyProductName());
        m_snapshot.insert(QStringLiteral("agentVersion"), QString::fromUtf8(kAppVersion));
        m_snapshot.insert(QStringLiteral("history"), m_history);
        m_lastError.clear();
        emit statusChanged(QStringLiteral("Live"));
        emit updated();
    } else {
        if (!today.ok) {
            finish();
            return;
        }
        const bool resolveProjects = m_settings.value(QStringLiteral("projectsEnabled")).toBool(true);
        auto fresh = extractUsageFromTokscale(today.json, resolveProjects);
        fresh = mergeLocalParsers(fresh, clients, QStringLiteral("today"));
        m_month = applyPeriodDelta(m_month, fresh, m_anchorToday).toObject();
        m_allTime = applyPeriodDelta(m_allTime, fresh, m_anchorToday).toObject();
        m_today = fresh;
        m_anchorToday = fresh;
        persistAnchor();
        mergeHistoryDay(m_today);
        persistHistory();
        m_snapshot.insert(QStringLiteral("today"), m_today);
        m_snapshot.insert(QStringLiteral("month"), m_month);
        m_snapshot.insert(QStringLiteral("allTime"), m_allTime);
        m_snapshot.insert(QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
        m_snapshot.insert(QStringLiteral("history"), m_history);
        emit updated();
    }
    finish();
}

bool UsageEngine::fullScan()
{
    emit statusChanged(QStringLiteral("Scanning"));
    maybeSelfSync(trackedClients(), m_runner.binary());
    const auto clients = trackedClients();
    const auto todayScanResult = m_runner.scanToday(clients);
    const auto monthScanResult = m_runner.scanMonth(clients);
    const auto since = m_settings.value(QStringLiteral("allTimeSince")).toString(QStringLiteral("2024-01-01"));
    const auto allScanResult = m_runner.scanSince(clients, since);
    QJsonObject wsl;
    if (m_settings.value(QStringLiteral("wslScanEnabled")).toBool(true))
        wsl = scanWslUsage(clients, since);
    const qint64 gen = ++m_generation;
    applyWorkerScan(true, gen, clients, todayScanResult, monthScanResult, allScanResult, wsl, {});
    return m_lastError.isEmpty();
}

bool UsageEngine::todayScan()
{
    if (m_anchorDate != QDate::currentDate() || m_anchorToday.isEmpty()) return fullScan();
    maybeSelfSync(trackedClients(), m_runner.binary(), true);
    const auto clients = trackedClients();
    const auto result = m_runner.scanToday(clients);
    const qint64 gen = ++m_generation;
    applyWorkerScan(false, gen, clients, result, {}, {}, {}, {});
    return result.ok;
}

void UsageEngine::applyGraphHistory(const QJsonObject &graph)
{
    // The graph recomputes every day from tool logs, so its rows are
    // authoritative for the range it covers; days outside it (older than the
    // retained window) keep their existing records.
    auto merged = m_history.value(QStringLiteral("days")).toObject();
    const auto days = graphHistoryDays(graph);
    for (auto it = days.begin(); it != days.end(); ++it)
        merged.insert(it.key(), it.value());
    m_history.insert(QStringLiteral("days"), merged);
}

void UsageEngine::persistAnchor()
{
    writeJsonAtomic(Paths::collectorAnchorPath(), QJsonObject{
        {QStringLiteral("date"), m_anchorDate.toString(Qt::ISODate)},
        {QStringLiteral("today"), m_anchorToday},
        {QStringLiteral("month"), m_month},
        {QStringLiteral("allTime"), m_allTime}
    });
}

void UsageEngine::mergeHistoryDay(const QJsonObject &today)
{
    auto days = m_history.value(QStringLiteral("days")).toObject();
    const auto key = QDate::currentDate().toString(Qt::ISODate);
    auto rec = QJsonObject{
        {QStringLiteral("tokens"), today.value(QStringLiteral("totalTokens"))},
        {QStringLiteral("costUsd"), today.value(QStringLiteral("costUsd"))},
        {QStringLiteral("clients"), today.value(QStringLiteral("clients"))},
        {QStringLiteral("models"), today.value(QStringLiteral("models"))}
    };
    const auto prev = days.value(key).toObject();
    if (prev.contains(QStringLiteral("activeTimeMs")))
        rec.insert(QStringLiteral("activeTimeMs"), prev.value(QStringLiteral("activeTimeMs")));
    days.insert(key, rec);
    const auto cutoff = QDate::currentDate().addDays(-400).toString(Qt::ISODate);
    for (const auto &day : days.keys()) {
        if (day < cutoff) days.remove(day);
    }
    m_history.insert(QStringLiteral("days"), days);
}

void UsageEngine::persistHistory()
{
    writeJsonAtomic(Paths::historyPath(), m_history);
}

} // namespace tmon
