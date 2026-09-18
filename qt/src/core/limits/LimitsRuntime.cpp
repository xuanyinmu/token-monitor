#include "core/limits/LimitsRuntime.h"

#include "core/catalog/Catalog.h"
#include "core/io/JsonIo.h"
#include "core/io/Paths.h"
#include "core/limits/Providers.h"
#include "core/net/HttpClient.h"
#include "core/tmon.h"

#include <QDateTime>
#include <QJsonArray>
#include <QPointer>
#include <QtConcurrent>
#include <QTimer>

namespace tmon {

LimitsRuntime::LimitsRuntime(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &LimitsRuntime::tick);
}

void LimitsRuntime::configure(const QJsonObject &settings)
{
    m_settings = settings;
    m_timer->setInterval(intervalMs());
    const auto cached = readJsonObject(Paths::limitsSnapshotPath());
    if (!cached.isEmpty() && m_snapshot.isEmpty()) {
        m_snapshot = cached;
        const auto providers = cached.value(QStringLiteral("providers")).toArray();
        for (const auto &pV : providers) {
            const auto row = pV.toObject();
            const auto id = row.value(QStringLiteral("provider")).toString();
            if (id.isEmpty()) continue;
            m_rows.insert(id, row);
            m_lastGood.insert(id, row);
        }
        emit updated();
    }
}

void LimitsRuntime::start()
{
    m_running = true;
    if (!m_settings.value(QStringLiteral("limitsEnabled")).toBool(true)) return;
    tick();
}

void LimitsRuntime::stop()
{
    m_running = false;
    m_timer->stop();
    m_queue.clear();
}

void LimitsRuntime::refresh()
{
    if (!m_running && m_settings.value(QStringLiteral("limitsEnabled")).toBool(true))
        m_running = true;
    tick();
}

int LimitsRuntime::intervalMs() const
{
    const int base = qMax(15'000, m_settings.value(QStringLiteral("limitsRefreshMs")).toInt(kDefaultLimitsRefreshMs));
    if (m_settings.value(QStringLiteral("limitsRefreshMode")).toString() != QLatin1String("adaptive"))
        return base;
    return suggestedAdaptiveMs(m_burn, base);
}

QStringList LimitsRuntime::enabledIds() const
{
    auto ids = m_settings.value(QStringLiteral("limitProviders")).toString(defaultLimitProvidersCsv())
                   .split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (auto &id : ids) id = id.trimmed();
    ids.removeAll(QString());
    return ids;
}

void LimitsRuntime::tick()
{
    if (!m_running || !m_settings.value(QStringLiteral("limitsEnabled")).toBool(true)) return;
    const auto ids = enabledIds();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const auto &id : ids) {
        if (m_inFlight.contains(id) || m_queue.contains(id)) continue;
        if (m_retryAt.value(id, 0) > now) continue;
        m_queue.append(id);
    }
    pump();
}

void LimitsRuntime::pump()
{
    while (m_active < kLimitsConcurrency && !m_queue.isEmpty()) {
        const auto id = m_queue.takeFirst();
        if (m_inFlight.contains(id)) continue;
        m_inFlight.insert(id);
        ++m_active;
        const auto settings = m_settings;
        QPointer<LimitsRuntime> self(this);
        QtConcurrent::run([self, id, settings]() {
            if (!self) return;
            HttpClient http;
            SpendStore spend;
            const auto row = fetchLimitProvider(id, settings, http, spend);
            if (!self) return;
            QMetaObject::invokeMethod(self, [self, id, row]() {
                if (self) self->onFetched(id, row);
            }, Qt::QueuedConnection);
        });
    }
    finishIfIdle();
}

void LimitsRuntime::onFetched(const QString &id, const QJsonObject &incoming)
{
    m_inFlight.remove(id);
    m_active = qMax(0, m_active - 1);
    auto row = incoming;
    const auto status = row.value(QStringLiteral("status")).toString();
    if (status == QLatin1String("ok")
        && row.value(QStringLiteral("windows")).toArray().isEmpty()
        && m_lastGood.contains(id)
        && !m_lastGood.value(id).value(QStringLiteral("windows")).toArray().isEmpty()) {
        row = m_lastGood.value(id);
    }
    if (status == QLatin1String("ok")) {
        m_lastGood.insert(id, row);
        m_attempts.remove(id);
        m_retryAt.remove(id);
        markLimitsProbeSuccess(m_burn, row);
    } else if (isRetryableLimitStatus(status) && m_lastGood.contains(id)) {
        auto kept = m_lastGood.value(id);
        kept.insert(QStringLiteral("status"), status);
        kept.insert(QStringLiteral("updatedAt"), row.value(QStringLiteral("updatedAt")));
        row = kept;
        const int attempt = m_attempts.value(id, 0) + 1;
        m_attempts.insert(id, attempt);
        m_retryAt.insert(id, QDateTime::currentMSecsSinceEpoch() + computeRetryDelayMs(attempt));
    }
    m_rows.insert(id, row);
    publish();
    pump();
}

void LimitsRuntime::finishIfIdle()
{
    if (m_active > 0 || !m_queue.isEmpty()) return;
    const int ms = intervalMs();
    m_timer->setInterval(ms);
    if (m_running && m_settings.value(QStringLiteral("limitsEnabled")).toBool(true))
        m_timer->start();
}

void LimitsRuntime::publish()
{
    QJsonArray providers;
    for (const auto &id : enabledIds()) {
        if (m_rows.contains(id)) providers.append(m_rows.value(id));
        else if (m_lastGood.contains(id)) providers.append(m_lastGood.value(id));
    }
    m_snapshot = QJsonObject{
        {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("refreshMs"), intervalMs()},
        {QStringLiteral("providers"), providers}
    };
    recordLimitsSample(m_burn, m_snapshot, QDateTime::currentMSecsSinceEpoch());
    writeJsonAtomic(Paths::limitsSnapshotPath(), m_snapshot);
    emit updated();
}

} // namespace tmon
