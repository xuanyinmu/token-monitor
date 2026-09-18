#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>
#include <QUrl>

namespace tmon {

struct HttpResult {
    int status = 0;
    QByteArray body;
    QString error;
    QJsonDocument json() const;
};

class HttpClient : public QObject {
    Q_OBJECT
public:
    explicit HttpClient(QObject *parent = nullptr);
    HttpResult get(const QUrl &url, const QMap<QString, QString> &headers = {}, int timeoutMs = 15000);
    HttpResult post(const QUrl &url, const QByteArray &body, const QMap<QString, QString> &headers = {}, int timeoutMs = 15000);
    HttpResult request(const QByteArray &method, const QUrl &url, const QByteArray &body,
                       const QMap<QString, QString> &headers, int timeoutMs);

private:
    QNetworkAccessManager m_nam;
};

QString statusForHttp(int code);
QString cleanSecret(const QString &value);

} // namespace tmon
