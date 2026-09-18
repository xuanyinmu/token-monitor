#include "core/hub/HubServer.h"
#include "core/io/Paths.h"
#include "core/tmon.h"
#include "core/usage/JsonUtil.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    tmon::HubStore store;
    store.setPath(QDir::temp().filePath(QStringLiteral("tmon-hub-test.json")));
    tmon::HubServer server(&store);
    if (!server.listen(0, QStringLiteral("127.0.0.1"), QString())) {
        std::cerr << "listen failed\n";
        return 1;
    }
    const auto rec = server.ingestLocal(QJsonObject{
        {QStringLiteral("deviceId"), QStringLiteral("dev-1")},
        {QStringLiteral("today"), tmon::emptyPeriod()}
    });
    if (rec.value(QStringLiteral("deviceId")).toString() != QLatin1String("dev-1")) {
        std::cerr << "ingest failed\n";
        return 1;
    }
    const auto first = store.setSubscriptions(QJsonArray{QJsonObject{{QStringLiteral("name"), QStringLiteral("pro")}}}, QString());
    const auto stale = store.setSubscriptions(QJsonArray{}, QStringLiteral("not-the-version"));
    if (stale.value(QStringLiteral("error")).toString() != QLatin1String("stale_write")) {
        std::cerr << "stale write did not 409\n";
        return 1;
    }
    Q_UNUSED(first);

    QNetworkAccessManager nam;
    QNetworkRequest req(QUrl(QStringLiteral("http://127.0.0.1:%1/api/health").arg(server.port())));
    auto *reply = nam.get(req);
    QObject::connect(reply, &QNetworkReply::finished, [&]() {
        const auto doc = QJsonDocument::fromJson(reply->readAll()).object();
        if (doc.value(QStringLiteral("runtime")).toString() != QLatin1String("native")) {
            std::cerr << "health runtime mismatch\n";
            QCoreApplication::exit(1);
            return;
        }
        QNetworkRequest putReq(QUrl(QStringLiteral("http://127.0.0.1:%1/api/subscriptions").arg(server.port())));
        putReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        const auto body = QJsonDocument(QJsonObject{
            {QStringLiteral("subscriptions"), QJsonArray{}},
            {QStringLiteral("baseUpdatedAt"), QStringLiteral("wrong")}
        }).toJson(QJsonDocument::Compact);
        auto *put = nam.put(putReq, body);
        QObject::connect(put, &QNetworkReply::finished, [put]() {
            const int code = put->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (code != 409) {
                std::cerr << "expected 409 got " << code << "\n";
                QCoreApplication::exit(1);
                return;
            }
            std::cout << "hub_protocol_check ok\n";
            QCoreApplication::exit(0);
        });
    });
    QTimer::singleShot(5000, [&]() { QCoreApplication::exit(2); });
    return app.exec();
}
