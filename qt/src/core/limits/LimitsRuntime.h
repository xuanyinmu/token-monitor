#pragma once

#include "core/limits/LimitsBurnRate.h"
#include "core/limits/SpendStore.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QStringList>

class QTimer;

namespace tmon {

class LimitsRuntime : public QObject {
    Q_OBJECT
public:
    explicit LimitsRuntime(QObject *parent = nullptr);
    void configure(const QJsonObject &settings);
    void start();
    void stop();
    void refresh();
    QJsonObject snapshot() const { return m_snapshot; }

signals:
    void updated();

private:
    void tick();
    void pump();
    void onFetched(const QString &id, const QJsonObject &row);
    void finishIfIdle();
    void publish();
    int intervalMs() const;
    QStringList enabledIds() const;
    QJsonObject m_settings;
    QJsonObject m_snapshot;
    QHash<QString, QJsonObject> m_rows;
    QHash<QString, QJsonObject> m_lastGood;
    QHash<QString, int> m_attempts;
    QHash<QString, qint64> m_retryAt;
    QSet<QString> m_inFlight;
    QStringList m_queue;
    SpendStore m_spend;
    LimitsBurnState m_burn;
    QTimer *m_timer = nullptr;
    int m_active = 0;
    bool m_running = false;
};

} // namespace tmon
