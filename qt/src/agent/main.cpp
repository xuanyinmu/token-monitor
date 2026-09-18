#include "core/catalog/Catalog.h"
#include "core/device/DeviceRuntime.h"
#include "core/hub/HubClient.h"
#include "core/io/CredentialStore.h"
#include "core/io/PidFile.h"
#include "core/io/JsonIo.h"
#include "core/io/Paths.h"
#include "core/io/SettingsStore.h"
#include "core/tmon.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFile>
#include <QTimer>
#include <QUrl>
#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication::setApplicationName(QString::fromUtf8(tmon::kAppName) + QStringLiteral(" Agent"));
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption once(QStringLiteral("once"), QStringLiteral("Collect once and exit"));
    parser.addOption(once);
    parser.process(app);

    auto settings = tmon::CredentialStore::overlayOnto(tmon::SettingsStore::load());
    if (settings.value(QStringLiteral("hubUrl")).toString().isEmpty())
        settings.insert(QStringLiteral("hubUrl"), qEnvironmentVariable("TOKEN_MONITOR_HUB_URL", QStringLiteral("http://127.0.0.1:17321")));
    settings.insert(QStringLiteral("hubMode"), QStringLiteral("client"));

    tmon::writePidFile(tmon::Paths::agentPidPath(), QCoreApplication::applicationPid());

    tmon::DeviceRuntime runtime;
    runtime.configure(settings);
    QObject::connect(&runtime, &tmon::DeviceRuntime::updated, [&]() {
        tmon::HubClient client;
        client.connectToHub(QUrl(settings.value(QStringLiteral("hubUrl")).toString()),
                            settings.value(QStringLiteral("secret")).toString());
        client.postIngest(runtime.deviceRecord());
        if (parser.isSet(once)) QCoreApplication::quit();
    });
    runtime.start();
    runtime.refreshUsage();
    if (parser.isSet(once)) {
        QTimer::singleShot(180000, &app, &QCoreApplication::quit);
    }
    const int rc = app.exec();
    tmon::removePidFile(tmon::Paths::agentPidPath());
    return rc;
}
