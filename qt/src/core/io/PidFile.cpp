#include "core/io/PidFile.h"

#include "core/io/Paths.h"

#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace tmon {
namespace {

bool processAlive(qint64 pid)
{
    if (pid <= 0) return false;
#ifdef Q_OS_WIN
    HANDLE handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
    if (!handle) return false;
    DWORD code = 0;
    const bool ok = GetExitCodeProcess(handle, &code);
    CloseHandle(handle);
    return ok && code == STILL_ACTIVE;
#else
    return false;
#endif
}

} // namespace

bool writePidFile(const QString &path, qint64 pid)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    file.write(QByteArray::number(pid));
    return true;
}

bool pidFileAlive(const QString &path)
{
    QFile file(path);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return false;
    bool ok = false;
    const qint64 pid = file.readAll().trimmed().toLongLong(&ok);
    if (!ok) {
        file.close();
        QFile::remove(path);
        return false;
    }
    if (!processAlive(pid)) {
        file.close();
        QFile::remove(path);
        return false;
    }
    return true;
}

void removePidFile(const QString &path)
{
    QFile::remove(path);
}

bool agentPidAlive()
{
    return pidFileAlive(Paths::agentPidPath());
}

} // namespace tmon
