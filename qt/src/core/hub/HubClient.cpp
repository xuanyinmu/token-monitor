#include "core/hub/HubClient.h"

#include "core/net/HttpClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace tmon {

HubClient::HubClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
}

void HubClient::connectToHub(const QUrl &url, const QString &secret)
{
    disconnectFromHub();
    m_url = url;
    m_secret = secret;
    QUrl stream = url;
    auto path = stream.path();
    if (path.endsWith(QLatin1Char('/'))) path.chop(1);
    stream.setPath(path + QStringLiteral("/api/stats/stream"));
    QNetworkRequest req(stream);
    req.setRawHeader("Accept", "text/event-stream");
    req.setRawHeader("x-token-monitor-stream", "2");
    if (!secret.isEmpty()) req.setRawHeader("Authorization", "Bearer " + secret.toUtf8());
    m_stream = m_nam->get(req);
    connect(m_stream, &QNetworkReply::readyRead, this, [this]() {
        m_connected = true;
        emit connectionChanged(true);
        m_buffer += m_stream->readAll();
        while (true) {
            const auto idx = m_buffer.indexOf("\n\n");
            if (idx < 0) break;
            const auto block = m_buffer.left(idx);
            m_buffer.remove(0, idx + 2);
            QByteArray data;
            for (const auto &line : block.split('\n')) {
                if (line.startsWith("data:")) data = line.mid(5).trimmed();
            }
            if (data.isEmpty()) continue;
            const auto doc = QJsonDocument::fromJson(data);
            auto obj = doc.object();
            auto stats = obj.value(QStringLiteral("stats")).toObject();
            if (stats.isEmpty()) stats = obj;
            if (!stats.isEmpty()) {
                m_stats = stats;
                emit statsReceived(stats);
            }
        }
    });
    connect(m_stream, &QNetworkReply::finished, this, [this]() {
        m_connected = false;
        emit connectionChanged(false);
        m_stream->deleteLater();
        m_stream = nullptr;
    });
}

void HubClient::disconnectFromHub()
{
    if (m_stream) {
        m_stream->abort();
        m_stream->deleteLater();
        m_stream = nullptr;
    }
    m_connected = false;
}

bool HubClient::postIngest(const QJsonObject &record)
{
    QUrl ingest = m_url;
    auto path = ingest.path();
    if (path.endsWith(QLatin1Char('/'))) path.chop(1);
    ingest.setPath(path + QStringLiteral("/api/ingest"));
    HttpClient http;
    QMap<QString, QString> headers{{QStringLiteral("Content-Type"), QStringLiteral("application/json")}};
    if (!m_secret.isEmpty()) headers.insert(QStringLiteral("Authorization"), QStringLiteral("Bearer ") + m_secret);
    headers.insert(QStringLiteral("x-token-monitor-response"), QStringLiteral("minimal"));
    const auto res = http.post(ingest, QJsonDocument(record).toJson(QJsonDocument::Compact), headers, 15000);
    return res.status == 200;
}

QJsonObject HubClient::putSubscriptions(const QJsonArray &list, const QString &baseUpdatedAt)
{
    QUrl dest = m_url;
    auto path = dest.path();
    if (path.endsWith(QLatin1Char('/'))) path.chop(1);
    dest.setPath(path + QStringLiteral("/api/subscriptions"));
    HttpClient http;
    QMap<QString, QString> headers{{QStringLiteral("Content-Type"), QStringLiteral("application/json")}};
    if (!m_secret.isEmpty()) headers.insert(QStringLiteral("Authorization"), QStringLiteral("Bearer ") + m_secret);
    const auto body = QJsonDocument(QJsonObject{
        {QStringLiteral("subscriptions"), list},
        {QStringLiteral("baseUpdatedAt"), baseUpdatedAt}
    }).toJson(QJsonDocument::Compact);
    const auto res = http.request("PUT", dest, body, headers, 15000);
    auto obj = res.json().object();
    obj.insert(QStringLiteral("httpStatus"), res.status);
    return obj;
}

} // namespace tmon
