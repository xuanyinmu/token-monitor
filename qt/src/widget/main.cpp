#include "core/tmon.h"
#include "core/catalog/Catalog.h"
#include "core/io/Paths.h"
#include "core/io/SettingsStore.h"
#include "core/usage/UsageEngine.h"
#include "widget/AppState.h"
#include "widget/DashboardWindow.h"
#include "widget/MaskIconProvider.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSslSocket>
#include <QTimer>
#include <cstdlib>
#include <iostream>

int main(int argc, char *argv[])
{
    qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
    QQuickWindow::setDefaultAlphaBuffer(true);
    QCoreApplication::setOrganizationName(QStringLiteral("Javis"));
    QCoreApplication::setApplicationName(QString::fromUtf8(tmon::kAppName));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(tmon::kAppVersion));
    QApplication app(argc, argv);
    QSslSocket::supportsSsl();
    { QNetworkAccessManager sslWarmup; Q_UNUSED(sslWarmup); }

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Token Monitor Qt"));
    parser.addHelpOption();
    QCommandLineOption scanOnce(QStringLiteral("scan-once"), QStringLiteral("Run one usage scan and print JSON"));
    parser.addOption(scanOnce);
    QCommandLineOption screenshotOpt(QStringLiteral("screenshot"), QStringLiteral("Grab the widget to PNG after first paint"), QStringLiteral("path"));
    parser.addOption(screenshotOpt);
    QCommandLineOption viewOpt(QStringLiteral("view"), QStringLiteral("Initial breakdown view"), QStringLiteral("name"));
    parser.addOption(viewOpt);
    QCommandLineOption dashOpt(QStringLiteral("open-dashboard"), QStringLiteral("Open the usage dashboard after launch"));
    parser.addOption(dashOpt);
    parser.process(app);

    if (parser.isSet(scanOnce)) {
        tmon::UsageEngine engine;
        engine.configure(tmon::SettingsStore::load());
        const bool ok = engine.scanOnce();
        if (!ok) {
            std::cerr << engine.lastError().toStdString() << std::endl;
        }
        const auto snap = engine.snapshot();
        std::cout << QJsonDocument(snap).toJson(QJsonDocument::Compact).constData() << std::endl;
        return ok ? 0 : 1;
    }

    tmon::AppState state;
    if (parser.isSet(screenshotOpt))
        state.setPreviewOnly(true);
    if (parser.isSet(viewOpt)) {
        const auto v = parser.value(viewOpt);
        if (v == QLatin1String("settings")) {
            state.forceView(QStringLiteral("limits"));
            state.setSettingsOpen(true);
        } else {
            state.forceView(v);
        }
    }
    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("mask"), new tmon::MaskIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &state);
    const QUrl url(QStringLiteral("qrc:/qt/qml/TokenMonitor/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [&](QObject *obj, const QUrl &) {
        if (!obj) QCoreApplication::exit(-1);
        auto *window = qobject_cast<QQuickWindow *>(obj);
        if (window) {
            window->setTitle(QString::fromUtf8(tmon::kAppName));
            if (parser.isSet(screenshotOpt)) {
                const auto path = parser.value(screenshotOpt);
                const auto viewName = parser.value(viewOpt);
                auto *timer = new QTimer(&app);
                int tries = 0;
                QObject::connect(timer, &QTimer::timeout, &app, [&state, path, timer, &tries, viewName]() {
                    ++tries;
                    bool ready = viewName == QLatin1String("settings");
                    if (!ready) {
                        for (const auto &row : state.limitRows()) {
                            if (!row.toMap().value(QStringLiteral("windows")).toList().isEmpty()) {
                                ready = true;
                                break;
                            }
                        }
                    }
                    if (ready || tries >= 24) {
                        timer->stop();
                        QTimer::singleShot(400, [path, &state]() {
                            state.saveScreenshot(path);
                            std::_Exit(0);
                        });
                    }
                });
                timer->start(250);
            }
            state.attachWindow(window);
            if (parser.isSet(dashOpt))
                QTimer::singleShot(800, &state, &tmon::AppState::showDashboard);
        }
    });
    engine.load(url);

    tmon::DashboardWindow dashboard(&state);
    QObject::connect(&state, &tmon::AppState::dashboardRequested, &dashboard, [&]() {
        dashboard.show();
    });

    return app.exec();
}
