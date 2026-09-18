#pragma once

#include "core/usage/TokscaleRunner.h"
#include "core/usage/UsageWatcher.h"

#include <QDate>
#include <QJsonObject>
#include <QObject>
#include <QStringList>

class QTimer;

namespace tmon {

class UsageEngine : public QObject {
    Q_OBJECT
public:
    explicit UsageEngine(QObject *parent = nullptr);
    void configure(const QJsonObject &settings);
    void start();
    void stop();
    void hydrateFromDisk();
    QJsonObject snapshot() const { return m_snapshot; }
    QJsonObject history() const { return m_history; }
    bool scanOnce();
    void requestScan(bool full);
    QString lastError() const { return m_lastError; }

signals:
    void updated();
    void statusChanged(const QString &text);

private:
    bool fullScan();
    bool todayScan();
    void beginScan(bool full);
    void applyTodayPartial(qint64 gen, const QStringList &clients, TokscaleScan today);
    void applyWorkerScan(bool full, qint64 gen, const QStringList &clients,
                         TokscaleScan today, TokscaleScan month, TokscaleScan allTime,
                         QJsonObject wsl);
    void persistAnchor();
    void persistHistory();
    void mergeHistoryDay(const QJsonObject &today);
    QStringList trackedClients() const;
    QDate m_anchorDate;
    QJsonObject m_settings;
    QJsonObject m_snapshot;
    QJsonObject m_today;
    QJsonObject m_month;
    QJsonObject m_allTime;
    QJsonObject m_anchorToday;
    QJsonObject m_history;
    QString m_lastError;
    qint64 m_generation = 0;
    TokscaleRunner m_runner;
    UsageWatcher *m_watcher = nullptr;
    QTimer *m_interval = nullptr;
    bool m_running = false;
    bool m_scanBusy = false;
    bool m_pendingFull = false;
    bool m_pendingToday = false;
};

} // namespace tmon
