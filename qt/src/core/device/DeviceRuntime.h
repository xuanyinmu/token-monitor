#pragma once

#include "core/hub/HubClient.h"
#include "core/hub/HubServer.h"
#include "core/limits/LimitsRuntime.h"
#include "core/usage/UsageEngine.h"

#include <QJsonObject>
#include <QObject>

namespace tmon {

class DeviceRuntime : public QObject {
    Q_OBJECT
public:
    explicit DeviceRuntime(QObject *parent = nullptr);
    void configure(const QJsonObject &settings);
    void start(bool usage = true);
    void stop();
    void refreshUsage();
    void refreshLimits();
    QJsonObject deviceRecord() const;
    QJsonObject displayStats() const;
    UsageEngine *usage() { return &m_usage; }
    LimitsRuntime *limits() { return &m_limits; }
    HubServer *embeddedHub() { return m_hubServer; }
    HubClient *hubClient() { return &m_hubClient; }

signals:
    void updated();
    void statusChanged(const QString &text);

private:
    void compose();
    void maybeUpload();
    QJsonObject m_settings;
    UsageEngine m_usage;
    LimitsRuntime m_limits;
    HubStore m_hubStore;
    HubServer *m_hubServer = nullptr;
    HubClient m_hubClient;
    QJsonObject m_remoteStats;
    QString m_mode;
};

} // namespace tmon
