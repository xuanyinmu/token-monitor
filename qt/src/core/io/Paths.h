#pragma once

#include <QString>

namespace tmon {

class Paths {
public:
    static QString homeDir();
    static QString userDataDir();
    static QString settingsPath();
    static QString credentialsPath();
    static QString hubDevicesPath();
    static QString collectorAnchorPath();
    static QString historyPath();
    static QString limitsSnapshotPath();
    static QString agentPidPath();
    static QString standaloneHubDevicesPath();
    static QString tokscaleBinary();
    static QString sharedDataDir();
    static QString xdgDataHome();
    static QString appDataRoaming();
    static QString appDataLocal();
    static QString envOr(const QString &name, const QString &fallback);
};

} // namespace tmon
