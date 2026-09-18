#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

namespace tmon {

class HubClient : public QObject {
    Q_OBJECT
public:
    explicit HubClient(QObject *parent = nullptr);
    void connectToHub(const QUrl &url, const QString &secret);
    void disconnectFromHub();
    bool postIngest(const QJsonObject &record);
    QJsonObject putSubscriptions(const QJsonArray &list, const QString &baseUpdatedAt);
    QJsonObject lastStats() const { return m_stats; }
    bool connected() const { return m_connected; }

signals:
    void statsReceived(const QJsonObject &stats);
    void connectionChanged(bool ok);

private:
    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_stream = nullptr;
    QUrl m_url;
    QString m_secret;
    QJsonObject m_stats;
    bool m_connected = false;
    QByteArray m_buffer;
};

} // namespace tmon
