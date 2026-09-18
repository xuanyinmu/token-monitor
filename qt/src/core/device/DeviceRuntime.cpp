#include "core/device/DeviceRuntime.h"

#include "core/io/PidFile.h"
#include "core/io/Paths.h"
#include "core/tmon.h"
#include "core/usage/UsageNormalize.h"

#include <QDateTime>
#include <QFile>
#include <QHostInfo>
#include <QJsonArray>
#include <QUrl>

namespace tmon {

DeviceRuntime::DeviceRuntime(QObject *parent)
    : QObject(parent)
    , m_hubServer(new HubServer(&m_hubStore, this))
{
    connect(&m_usage, &UsageEngine::updated, this, &DeviceRuntime::compose);
    connect(&m_usage, &UsageEngine::statusChanged, this, &DeviceRuntime::statusChanged);
    connect(&m_limits, &LimitsRuntime::updated, this, &DeviceRuntime::compose);
    connect(&m_hubClient, &HubClient::statsReceived, this, [this](const QJsonObject &stats) {
        m_remoteStats = stats;
        emit updated();
    });
}

void DeviceRuntime::configure(const QJsonObject &settings)
{
    m_settings = settings;
    m_usage.configure(settings);
    m_limits.configure(settings);
    m_mode = settings.value(QStringLiteral("hubMode")).toString(QStringLiteral("local"));
}

void DeviceRuntime::start(bool usage)
{
    // Limits HTTP must not wait behind tokscale: the GUI thread used to run
    // a blocking fullScan first, so the live Limits page stayed on the last
    // empty stub until the collector returned.
    m_limits.start();
    if (usage) m_usage.start();
    if (m_mode == QLatin1String("host")) {
        m_hubStore.setPath(Paths::hubDevicesPath());
        m_hubServer->listen(quint16(m_settings.value(QStringLiteral("hubHostPort")).toInt(kHubDefaultPort)),
                            QStringLiteral("0.0.0.0"),
                            m_settings.value(QStringLiteral("hubHostSecret")).toString());
    } else if (m_mode == QLatin1String("client")) {
        const auto url = QUrl(m_settings.value(QStringLiteral("hubUrl")).toString());
        m_hubClient.connectToHub(url, m_settings.value(QStringLiteral("secret")).toString());
    }
}

void DeviceRuntime::stop()
{
    m_usage.stop();
    m_limits.stop();
    m_hubServer->stop();
    m_hubClient.disconnectFromHub();
}

void DeviceRuntime::refreshUsage() { m_usage.requestScan(true); }
void DeviceRuntime::refreshLimits() { m_limits.refresh(); }

QJsonObject DeviceRuntime::deviceRecord() const
{
    auto rec = m_usage.snapshot();
    rec.insert(QStringLiteral("limits"), m_limits.snapshot());
    rec.insert(QStringLiteral("agentRuntime"), QStringLiteral("qt-widget"));
    rec.insert(QStringLiteral("periods"), QJsonObject{
        {QStringLiteral("today"), rec.value(QStringLiteral("today"))},
        {QStringLiteral("month"), rec.value(QStringLiteral("month"))},
        {QStringLiteral("allTime"), rec.value(QStringLiteral("allTime"))}
    });
    return rec;
}

QJsonObject DeviceRuntime::displayStats() const
{
    if (m_mode == QLatin1String("client") && !m_remoteStats.isEmpty()) return m_remoteStats;
    if (m_mode == QLatin1String("host")) return m_hubStore.stats(kDefaultStaleAfterMs);
    auto local = deviceRecord();
    QJsonArray devices{local};
    return aggregateDevices(devices, kDefaultStaleAfterMs);
}

void DeviceRuntime::compose()
{
    maybeUpload();
    emit updated();
}

void DeviceRuntime::maybeUpload()
{
    const auto rec = deviceRecord();
    if (m_mode == QLatin1String("host")) {
        m_hubServer->ingestLocal(rec);
    } else if (m_mode == QLatin1String("client")) {
        if (agentPidAlive()) return;
        m_hubClient.postIngest(rec);
    }
}

} // namespace tmon
