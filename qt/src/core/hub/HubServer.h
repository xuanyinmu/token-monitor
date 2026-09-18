#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QVector>

namespace tmon {

class HubStore : public QObject {
    Q_OBJECT
public:
    explicit HubStore(QObject *parent = nullptr);
    void setPath(const QString &path);
    QJsonObject ingest(const QJsonObject &payload);
    bool deleteDevice(const QString &id);
    QJsonObject stats(int staleAfterMs) const;
    QJsonArray devices() const;
    QJsonObject subscriptions() const;
    QJsonObject setSubscriptions(const QJsonArray &list, const QString &baseUpdatedAt);
    QJsonObject history() const;
    int deviceCount() const;

signals:
    void changed(const QString &reason);

private:
    void load();
    void persist();
    QString m_path;
    QJsonObject m_store;
};

class HubServer : public QObject {
    Q_OBJECT
public:
    explicit HubServer(HubStore *store, QObject *parent = nullptr);
    bool listen(quint16 port, const QString &host, const QString &secret);
    void stop();
    quint16 port() const;
    QJsonObject ingestLocal(const QJsonObject &payload);

private:
    void onConnection();
    void handle(QTcpSocket *socket);
    HubStore *m_store = nullptr;
    QTcpServer m_server;
    QString m_secret;
    struct SseClient {
        QTcpSocket *socket = nullptr;
        bool freshness = false;
    };
    QVector<SseClient> m_sse;
    QTimer *m_broadcast = nullptr;
    QTimer *m_heartbeat = nullptr;
    QString m_lastContentKey;
    void queueBroadcast();
    void flushBroadcast();
    void writeSse(QTcpSocket *socket, const QByteArray &event, const QJsonObject &data);
};

} // namespace tmon
