#include "core/tmon.h"
#include "core/hub/HubServer.h"
#include "core/io/Paths.h"
#include "core/io/SettingsStore.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(QString::fromUtf8(tmon::kAppName) + QStringLiteral(" Hub"));
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption portOpt(QStringLiteral("port"), QStringLiteral("Listen port"), QStringLiteral("port"));
    QCommandLineOption hostOpt(QStringLiteral("host"), QStringLiteral("Bind host"), QStringLiteral("host"));
    QCommandLineOption secretOpt(QStringLiteral("secret"), QStringLiteral("Shared secret"), QStringLiteral("secret"));
    parser.addOption(portOpt);
    parser.addOption(hostOpt);
    parser.addOption(secretOpt);
    parser.process(app);

    const auto settings = tmon::SettingsStore::load();
    const auto port = parser.isSet(portOpt) ? parser.value(portOpt).toUShort()
                                            : quint16(settings.value(QStringLiteral("hubHostPort")).toInt(tmon::kHubDefaultPort));
    const auto host = parser.isSet(hostOpt) ? parser.value(hostOpt) : QStringLiteral("0.0.0.0");
    const auto secret = parser.isSet(secretOpt) ? parser.value(secretOpt)
                                                : settings.value(QStringLiteral("hubHostSecret")).toString();

    tmon::HubStore store;
    store.setPath(tmon::Paths::standaloneHubDevicesPath());
    tmon::HubServer server(&store);
    if (!server.listen(port, host, secret)) {
        std::cerr << "failed to listen on " << port << std::endl;
        return 1;
    }
    std::cerr << "Token Monitor hub (native) on port " << server.port() << std::endl;
    return app.exec();
}
