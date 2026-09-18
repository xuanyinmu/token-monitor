#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace tmon {

struct SubprocessResult {
    int exitCode = -1;
    QByteArray stdoutBytes;
    QByteArray stderrBytes;
    bool timedOut = false;
    bool started = false;
};

SubprocessResult runProcess(const QString &program, const QStringList &args,
                            int timeoutMs, const QString &workingDir = {});

} // namespace tmon
