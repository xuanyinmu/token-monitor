#include "core/net/HttpClient.h"

#include <QEventLoop>
#include <QMap>
#include <QNetworkRequest>
#include <QTimer>

namespace tmon {

QJsonDocument HttpResult::json() const
{
    return QJsonDocument::fromJson(body);
}

HttpClient::HttpClient(QObject *parent)
    : QObject(parent)
{
}

HttpResult HttpClient::get(const QUrl &url, const QMap<QString, QString> &headers, int timeoutMs)
{
    return request("GET", url, {}, headers, timeoutMs);
}

HttpResult HttpClient::post(const QUrl &url, const QByteArray &body, const QMap<QString, QString> &headers, int timeoutMs)
{
    return request("POST", url, body, headers, timeoutMs);
}

HttpResult HttpClient::request(const QByteArray &method, const QUrl &url, const QByteArray &body,
                               const QMap<QString, QString> &headers, int timeoutMs)
{
    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    const bool hasUa = headers.contains(QStringLiteral("User-Agent"))
        || headers.contains(QStringLiteral("user-agent"));
    if (!hasUa) {
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36"));
    }
    for (auto it = headers.begin(); it != headers.end(); ++it)
        req.setRawHeader(it.key().toUtf8(), it.value().toUtf8());

    QNetworkReply *reply = m_nam.sendCustomRequest(req, method, body);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    HttpResult result;
    if (!timer.isActive()) {
        reply->abort();
        result.error = QStringLiteral("timeout");
        result.status = 0;
    } else {
        result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.body = reply->readAll();
        result.error = reply->error() == QNetworkReply::NoError ? QString() : reply->errorString();
    }
    reply->deleteLater();
    return result;
}

QString statusForHttp(int code)
{
    if (code == 401 || code == 403) return QStringLiteral("unauthorized");
    if (code == 429) return QStringLiteral("sourceRateLimited");
    return QStringLiteral("unavailable");
}

QString cleanSecret(const QString &value)
{
    auto raw = value.trimmed();
    if ((raw.startsWith(QLatin1Char('"')) && raw.endsWith(QLatin1Char('"')))
        || (raw.startsWith(QLatin1Char('\'')) && raw.endsWith(QLatin1Char('\'')))) {
        raw = raw.mid(1, raw.size() - 2).trimmed();
    }
    if (raw.contains(QLatin1Char('\n')) || raw.contains(QLatin1Char('\r'))) return {};
    return raw;
}

} // namespace tmon
