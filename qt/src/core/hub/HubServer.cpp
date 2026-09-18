#include "core/hub/HubServer.h"

#include "core/io/JsonIo.h"
#include "core/tmon.h"
#include "core/usage/UsageNormalize.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QMap>
#include <QTimer>
#include <QUrl>

namespace tmon {
namespace {

QByteArray sseFormat(const QByteArray &event, const QJsonObject &data)
{
    return "event: " + event + "\ndata: " + QJsonDocument(data).toJson(QJsonDocument::Compact) + "\n\n";
}

bool authorized(const QMap<QByteArray, QByteArray> &headers, const QString &secret)
{
    if (secret.isEmpty()) return true;
    const auto bearer = headers.value("authorization");
    if (bearer.startsWith("Bearer ") && QString::fromUtf8(bearer.mid(7)) == secret) return true;
    return QString::fromUtf8(headers.value("x-token-monitor-secret")) == secret;
}

QMap<QByteArray, QByteArray> parseHeaders(const QByteArray &block)
{
    QMap<QByteArray, QByteArray> headers;
    const auto lines = block.split('\n');
    for (int i = 1; i < lines.size(); ++i) {
        auto line = lines[i].trimmed();
        const auto idx = line.indexOf(':');
        if (idx < 0) continue;
        headers.insert(line.left(idx).trimmed().toLower(), line.mid(idx + 1).trimmed());
    }
    return headers;
}

} // namespace

HubStore::HubStore(QObject *parent)
    : QObject(parent)
{
    m_store = QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("devices"), QJsonObject{}},
        {QStringLiteral("subscriptions"), QJsonObject{
            {QStringLiteral("updatedAt"), QString()},
            {QStringLiteral("records"), QJsonArray{}}
        }}
    };
}

void HubStore::setPath(const QString &path)
{
    m_path = path;
    load();
}

void HubStore::load()
{
    auto stored = readJsonObject(m_path);
    if (!stored.isEmpty()) m_store = stored;
    if (!m_store.value(QStringLiteral("devices")).isObject())
        m_store.insert(QStringLiteral("devices"), QJsonObject{});
}

void HubStore::persist()
{
    m_store.insert(QStringLiteral("savedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    writeJsonAtomic(m_path, m_store);
}

QJsonObject HubStore::ingest(const QJsonObject &payload)
{
    auto devices = m_store.value(QStringLiteral("devices")).toObject();
    const auto id = payload.value(QStringLiteral("deviceId")).toString(payload.value(QStringLiteral("id")).toString());
    if (id.isEmpty()) return {};
    auto incoming = payload;
    incoming.insert(QStringLiteral("deviceId"), id);
    incoming.insert(QStringLiteral("receivedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    const auto existing = stripSessionTextFromDeviceRecord(devices.value(id).toObject());
    const auto record = mergeDeviceRecord(existing, stripSessionTextFromDeviceRecord(incoming));
    devices.insert(record.value(QStringLiteral("deviceId")).toString(), record);
    m_store.insert(QStringLiteral("devices"), devices);
    persist();
    emit changed(QStringLiteral("ingest"));
    return record;
}

bool HubStore::deleteDevice(const QString &id)
{
    auto devices = m_store.value(QStringLiteral("devices")).toObject();
    if (!devices.contains(id)) return false;
    devices.remove(id);
    m_store.insert(QStringLiteral("devices"), devices);
    persist();
    emit changed(QStringLiteral("delete"));
    return true;
}

QJsonArray HubStore::devices() const
{
    QJsonArray out;
    const auto map = m_store.value(QStringLiteral("devices")).toObject();
    for (auto it = map.begin(); it != map.end(); ++it) out.append(it.value());
    return out;
}

int HubStore::deviceCount() const
{
    return m_store.value(QStringLiteral("devices")).toObject().size();
}

QJsonObject HubStore::stats(int staleAfterMs) const
{
    auto aggregated = aggregateDevices(devices(), staleAfterMs);
    aggregated.insert(QStringLiteral("subscriptionsUpdatedAt"),
                      m_store.value(QStringLiteral("subscriptions")).toObject().value(QStringLiteral("updatedAt")).toString());
    return aggregated;
}

QJsonObject HubStore::subscriptions() const
{
    return m_store.value(QStringLiteral("subscriptions")).toObject();
}

QJsonObject HubStore::setSubscriptions(const QJsonArray &list, const QString &baseUpdatedAt)
{
    auto current = subscriptions();
    if (!baseUpdatedAt.isEmpty() && current.value(QStringLiteral("updatedAt")).toString() != baseUpdatedAt) {
        QJsonObject err{{QStringLiteral("error"), QStringLiteral("stale_write")}};
        err.insert(QStringLiteral("current"), current);
        return err;
    }
    const auto next = QJsonObject{
        {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {QStringLiteral("records"), list}
    };
    m_store.insert(QStringLiteral("subscriptions"), next);
    persist();
    emit changed(QStringLiteral("subscriptions"));
    return next;
}

QJsonObject HubStore::history() const
{
    QJsonObject days;
    const auto map = m_store.value(QStringLiteral("devices")).toObject();
    for (auto it = map.begin(); it != map.end(); ++it) {
        const auto hist = it.value().toObject().value(QStringLiteral("history")).toObject();
        const auto src = hist.value(QStringLiteral("days")).toObject();
        for (auto d = src.begin(); d != src.end(); ++d) {
            auto acc = days.value(d.key()).toObject();
            const auto val = d.value().toObject();
            acc.insert(QStringLiteral("tokens"), acc.value(QStringLiteral("tokens")).toDouble() + val.value(QStringLiteral("tokens")).toDouble());
            acc.insert(QStringLiteral("costUsd"), acc.value(QStringLiteral("costUsd")).toDouble() + val.value(QStringLiteral("costUsd")).toDouble());
            days.insert(d.key(), acc);
        }
    }
    return QJsonObject{{QStringLiteral("days"), days}};
}

HubServer::HubServer(HubStore *store, QObject *parent)
    : QObject(parent)
    , m_store(store)
    , m_broadcast(new QTimer(this))
    , m_heartbeat(new QTimer(this))
{
    connect(&m_server, &QTcpServer::newConnection, this, &HubServer::onConnection);
    m_broadcast->setSingleShot(true);
    m_broadcast->setInterval(kHubBroadcastDelayMs);
    connect(m_broadcast, &QTimer::timeout, this, &HubServer::flushBroadcast);
    m_heartbeat->setInterval(kSseHeartbeatMs);
    connect(m_heartbeat, &QTimer::timeout, this, [this]() {
        for (auto it = m_sse.begin(); it != m_sse.end();) {
            if (!it->socket || it->socket->state() != QAbstractSocket::ConnectedState) {
                it = m_sse.erase(it);
                continue;
            }
            it->socket->write(": hb\n\n");
            ++it;
        }
    });
    connect(m_store, &HubStore::changed, this, [this](const QString &) { queueBroadcast(); });
}

bool HubServer::listen(quint16 port, const QString &host, const QString &secret)
{
    m_secret = secret;
    QHostAddress addr = QHostAddress::Any;
    if (!secret.isEmpty()) {
        if (!host.isEmpty() && host != QLatin1String("0.0.0.0")) addr = QHostAddress(host);
    } else {
        addr = QHostAddress::LocalHost;
    }
    const bool ok = m_server.listen(addr, port);
    if (ok) m_heartbeat->start();
    return ok;
}

void HubServer::stop()
{
    m_heartbeat->stop();
    m_broadcast->stop();
    for (auto &c : m_sse) if (c.socket) c.socket->disconnectFromHost();
    m_sse.clear();
    m_server.close();
}

quint16 HubServer::port() const { return m_server.serverPort(); }

QJsonObject HubServer::ingestLocal(const QJsonObject &payload)
{
    return m_store->ingest(payload);
}

void HubServer::onConnection()
{
    while (m_server.hasPendingConnections()) {
        auto *socket = m_server.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { handle(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
    }
}

void HubServer::writeSse(QTcpSocket *socket, const QByteArray &event, const QJsonObject &data)
{
    if (!socket) return;
    socket->write(sseFormat(event, data));
}

void HubServer::queueBroadcast()
{
    if (m_sse.isEmpty() || m_broadcast->isActive()) return;
    m_broadcast->start();
}

void HubServer::flushBroadcast()
{
    const auto stats = m_store->stats(kDefaultStaleAfterMs);
    const auto key = QJsonDocument(stats.value(QStringLiteral("periods")).toObject()).toJson(QJsonDocument::Compact);
    const auto at = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    for (auto &c : m_sse) {
        if (!c.socket) continue;
        if (key != m_lastContentKey || !c.freshness) {
            writeSse(c.socket, "stats", QJsonObject{
                {QStringLiteral("type"), QStringLiteral("stats")},
                {QStringLiteral("reason"), QStringLiteral("ingest")},
                {QStringLiteral("stats"), stats},
                {QStringLiteral("at"), at}
            });
        } else {
            writeSse(c.socket, "freshness", QJsonObject{
                {QStringLiteral("type"), QStringLiteral("freshness")},
                {QStringLiteral("reason"), QStringLiteral("ingest")},
                {QStringLiteral("at"), at},
                {QStringLiteral("stats"), QJsonObject{{QStringLiteral("updatedAt"), stats.value(QStringLiteral("updatedAt"))}}}
            });
        }
    }
    m_lastContentKey = QString::fromUtf8(key);
}

void HubServer::handle(QTcpSocket *socket)
{
    const auto peek = socket->peek(64 * 1024);
    const auto headerEnd = peek.indexOf("\r\n\r\n");
    if (headerEnd < 0) return;
    const auto headerBlock = peek.left(headerEnd);
    const auto first = headerBlock.split('\n').value(0).trimmed();
    const auto parts = QString::fromUtf8(first).split(QLatin1Char(' '));
    if (parts.size() < 2) return;
    const auto method = parts[0];
    const auto path = QUrl(parts[1]).path();
    const auto headers = parseHeaders(headerBlock);
    const int contentLength = headers.value("content-length").toInt();
    const int total = headerEnd + 4 + contentLength;
    if (socket->bytesAvailable() < total && contentLength > 0) return;
    socket->read(headerEnd + 4);
    const auto body = contentLength > 0 ? socket->read(contentLength) : QByteArray();

    auto sendJson = [&](int code, const QJsonObject &obj) {
        const auto payload = QJsonDocument(obj).toJson(QJsonDocument::Compact);
        QByteArray resp = "HTTP/1.1 " + QByteArray::number(code) + " OK\r\n";
        if (code == 401) resp = "HTTP/1.1 401 Unauthorized\r\n";
        if (code == 409) resp = "HTTP/1.1 409 Conflict\r\n";
        if (code == 400) resp = "HTTP/1.1 400 Bad Request\r\n";
        if (code == 404) resp = "HTTP/1.1 404 Not Found\r\n";
        resp += "Content-Type: application/json\r\nContent-Length: " + QByteArray::number(payload.size()) + "\r\nConnection: close\r\n\r\n";
        resp += payload;
        socket->write(resp);
        socket->disconnectFromHost();
    };

    if (method == QLatin1String("OPTIONS")) {
        socket->write("HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n");
        socket->disconnectFromHost();
        return;
    }

    if (path == QLatin1String("/api/health")) {
        sendJson(200, QJsonObject{
            {QStringLiteral("ok"), true},
            {QStringLiteral("role"), QStringLiteral("hub")},
            {QStringLiteral("runtime"), QString::fromUtf8(kNativeRuntime)},
            {QStringLiteral("version"), 1},
            {QStringLiteral("deviceCount"), m_store->deviceCount()},
            {QStringLiteral("secretRequired"), !m_secret.isEmpty()},
            {QStringLiteral("now"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
        });
        return;
    }

    if (!authorized(headers, m_secret)) {
        sendJson(401, QJsonObject{{QStringLiteral("error"), QStringLiteral("unauthorized")}});
        return;
    }

    if (method == QLatin1String("GET") && path == QLatin1String("/api/stats")) {
        sendJson(200, m_store->stats(kDefaultStaleAfterMs));
        return;
    }
    if (method == QLatin1String("GET") && path == QLatin1String("/api/devices")) {
        sendJson(200, QJsonObject{{QStringLiteral("devices"), m_store->devices()}});
        return;
    }
    if (method == QLatin1String("GET") && path == QLatin1String("/api/history")) {
        sendJson(200, m_store->history());
        return;
    }
    if (method == QLatin1String("GET") && path == QLatin1String("/api/subscriptions")) {
        auto sub = m_store->subscriptions();
        sub.insert(QStringLiteral("ok"), true);
        sendJson(200, sub);
        return;
    }
    if (method == QLatin1String("GET") && path == QLatin1String("/api/stats/stream")) {
        QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache, no-transform\r\nConnection: keep-alive\r\nX-Accel-Buffering: no\r\n\r\n";
        socket->write(resp);
        const auto stats = m_store->stats(kDefaultStaleAfterMs);
        writeSse(socket, "snapshot", QJsonObject{
            {QStringLiteral("type"), QStringLiteral("stats")},
            {QStringLiteral("reason"), QStringLiteral("snapshot")},
            {QStringLiteral("stats"), stats},
            {QStringLiteral("at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}
        });
        SseClient client;
        client.socket = socket;
        client.freshness = headers.value("x-token-monitor-stream") == "2";
        m_sse.append(client);
        disconnect(socket, &QTcpSocket::readyRead, nullptr, nullptr);
        return;
    }
    if (method == QLatin1String("POST") && path == QLatin1String("/api/ingest")) {
        const auto doc = QJsonDocument::fromJson(body);
        const auto record = m_store->ingest(doc.object());
        if (record.isEmpty()) {
            sendJson(400, QJsonObject{{QStringLiteral("error"), QStringLiteral("deviceId_required")}});
            return;
        }
        QJsonObject response{{QStringLiteral("ok"), true}, {QStringLiteral("deviceId"), record.value(QStringLiteral("deviceId"))}};
        if (headers.value("x-token-monitor-response") != "minimal")
            response.insert(QStringLiteral("stats"), m_store->stats(kDefaultStaleAfterMs));
        sendJson(200, response);
        return;
    }
    if (method == QLatin1String("PUT") && path == QLatin1String("/api/subscriptions")) {
        const auto doc = QJsonDocument::fromJson(body).object();
        const auto stored = m_store->setSubscriptions(doc.value(QStringLiteral("subscriptions")).toArray(),
                                                      doc.value(QStringLiteral("baseUpdatedAt")).toString());
        if (stored.value(QStringLiteral("error")).toString() == QLatin1String("stale_write")) {
            sendJson(409, stored);
            return;
        }
        auto ok = stored;
        ok.insert(QStringLiteral("ok"), true);
        sendJson(200, ok);
        return;
    }
    if (method == QLatin1String("DELETE") && path.startsWith(QLatin1String("/api/devices/"))) {
        const auto id = path.mid(QStringLiteral("/api/devices/").size());
        m_store->deleteDevice(id);
        sendJson(200, QJsonObject{{QStringLiteral("ok"), true}});
        return;
    }
    sendJson(404, QJsonObject{{QStringLiteral("error"), QStringLiteral("not_found")}});
}

} // namespace tmon
