#include "core/io/Paths.h"

#include "core/tmon.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace tmon {

QString Paths::homeDir()
{
    const auto home = QDir::homePath();
    return home.isEmpty() ? QDir::rootPath() : home;
}

QString Paths::userDataDir()
{
    const auto override = qEnvironmentVariable("TOKEN_MONITOR_USER_DATA");
    if (!override.trimmed().isEmpty()) return override;
    return QDir(appDataRoaming()).filePath(QString::fromUtf8(kAppName));
}

QString Paths::settingsPath()
{
    return QDir(userDataDir()).filePath(QStringLiteral("settings.json"));
}

QString Paths::credentialsPath()
{
    return QDir(userDataDir()).filePath(QStringLiteral("credentials.json"));
}

QString Paths::hubDevicesPath()
{
    return QDir(userDataDir()).filePath(QStringLiteral("hub-devices.json"));
}

QString Paths::collectorAnchorPath()
{
    return QDir(userDataDir()).filePath(QStringLiteral("collector-anchor.json"));
}

QString Paths::historyPath()
{
    return QDir(userDataDir()).filePath(QStringLiteral("history.json"));
}

QString Paths::limitsSnapshotPath()
{
    return QDir(userDataDir()).filePath(QStringLiteral("limits-snapshot.json"));
}

QString Paths::agentPidPath()
{
    return QDir(userDataDir()).filePath(QStringLiteral("agent.pid"));
}

QString Paths::standaloneHubDevicesPath()
{
    return QDir(sharedDataDir()).filePath(QStringLiteral("devices.json"));
}

QString Paths::tokscaleBinary()
{
    const auto env = qEnvironmentVariable("TOKEN_MONITOR_TOKSCALE");
    if (!env.trimmed().isEmpty() && QFileInfo::exists(env)) return env;
#ifdef Q_OS_WIN
    const auto name = QStringLiteral("tokscale.exe");
#else
    const auto name = QStringLiteral("tokscale");
#endif
    const QString inUserData = QDir(userDataDir()).filePath(name);
    if (QFileInfo::exists(inUserData)) return inUserData;
    return QDir(QCoreApplication::applicationDirPath()).filePath(name);
}

QString Paths::sharedDataDir()
{
    const auto env = qEnvironmentVariable("TOKEN_MONITOR_DATA_DIR");
    if (!env.trimmed().isEmpty()) return env;
    return QDir(userDataDir()).filePath(QStringLiteral("data"));
}

QString Paths::xdgDataHome()
{
    const auto env = qEnvironmentVariable("XDG_DATA_HOME");
    if (!env.trimmed().isEmpty()) return env;
    return QDir(homeDir()).filePath(QStringLiteral(".local/share"));
}

QString Paths::appDataRoaming()
{
    const auto env = qEnvironmentVariable("APPDATA");
    if (!env.trimmed().isEmpty()) return env;
    return QDir(homeDir()).filePath(QStringLiteral("AppData/Roaming"));
}

QString Paths::appDataLocal()
{
    const auto env = qEnvironmentVariable("LOCALAPPDATA");
    if (!env.trimmed().isEmpty()) return env;
    return QDir(homeDir()).filePath(QStringLiteral("AppData/Local"));
}

QString Paths::envOr(const QString &name, const QString &fallback)
{
    const auto value = qEnvironmentVariable(name.toUtf8().constData()).trimmed();
    return value.isEmpty() ? fallback : value;
}

} // namespace tmon
