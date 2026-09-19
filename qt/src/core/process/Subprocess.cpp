#include "core/process/Subprocess.h"

#include "core/tmon.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QThread>

namespace tmon {

SubprocessResult runProcess(const QString &program, const QStringList &args,
                            int timeoutMs, const QString &workingDir,
                            const QMap<QString, QString> &extraEnv)
{
    SubprocessResult result;
    QProcess proc;
    if (!extraEnv.isEmpty()) {
        auto env = QProcessEnvironment::systemEnvironment();
        for (auto it = extraEnv.begin(); it != extraEnv.end(); ++it)
            env.insert(it.key(), it.value());
        proc.setProcessEnvironment(env);
    }
    if (!workingDir.isEmpty()) proc.setWorkingDirectory(workingDir);
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(program, args);
    if (!proc.waitForStarted(5000)) return result;
    result.started = true;
    QElapsedTimer timer;
    timer.start();
    while (proc.state() != QProcess::NotRunning) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 40);
        if (timer.elapsed() > timeoutMs) {
            result.timedOut = true;
            proc.terminate();
            QElapsedTimer grace;
            grace.start();
            while (proc.state() != QProcess::NotRunning && grace.elapsed() < kSubprocessGraceMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 40);
            }
            if (proc.state() != QProcess::NotRunning) {
                proc.kill();
                grace.restart();
                while (proc.state() != QProcess::NotRunning && grace.elapsed() < kSubprocessGraceMs)
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 40);
            }
            break;
        }
        QThread::msleep(15);
    }
    result.exitCode = proc.exitCode();
    result.stdoutBytes = proc.readAllStandardOutput();
    result.stderrBytes = proc.readAllStandardError();
    return result;
}

} // namespace tmon
