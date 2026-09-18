#pragma once

#include <QString>

namespace tmon {

bool writePidFile(const QString &path, qint64 pid);
bool pidFileAlive(const QString &path);
void removePidFile(const QString &path);
bool agentPidAlive();

} // namespace tmon
