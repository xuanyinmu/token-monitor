#include "core/usage/WslUsage.h"

#include "core/catalog/ClientRoots.h"
#include "core/io/Paths.h"
#include "core/process/Subprocess.h"
#include "core/tmon.h"
#include "core/usage/JsonUtil.h"
#include "core/usage/TokscaleRunner.h"
#include "core/usage/UsageNormalize.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace tmon {
namespace {

bool wslInstalled()
{
#ifdef Q_OS_WIN
    HKEY key = nullptr;
    const auto status = RegOpenKeyExW(HKEY_CURRENT_USER,
                                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Lxss",
                                      0, KEY_READ, &key);
    if (status != ERROR_SUCCESS) return false;
    RegCloseKey(key);
    return true;
#else
    return false;
#endif
}

QStringList runningDistros()
{
    const auto run = runProcess(QStringLiteral("wsl.exe"),
                                {QStringLiteral("--list"), QStringLiteral("--running"), QStringLiteral("--quiet")},
                                15000);
    QString text = QString::fromUtf16(reinterpret_cast<const char16_t *>(run.stdoutBytes.constData()),
                                      run.stdoutBytes.size() / 2);
    if (text.isEmpty()) text = QString::fromLocal8Bit(run.stdoutBytes);
    QStringList names;
    for (auto line : text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts)) {
        line = line.trimmed();
        if (!line.isEmpty()) names.append(line);
    }
    return names;
}

} // namespace

QJsonObject scanWslUsage(const QStringList &clients, const QString &allTimeSince)
{
    QJsonObject status{{QStringLiteral("state"), QStringLiteral("disabled")},
                       {QStringLiteral("detected"), QJsonArray{}},
                       {QStringLiteral("withData"), QJsonArray{}}};
    QJsonObject periods{
        {QStringLiteral("today"), emptyPeriod()},
        {QStringLiteral("month"), emptyPeriod()},
        {QStringLiteral("allTime"), emptyPeriod()}
    };
#ifndef Q_OS_WIN
    Q_UNUSED(clients);
    Q_UNUSED(allTimeSince);
    status.insert(QStringLiteral("state"), QStringLiteral("not-installed"));
    periods.insert(QStringLiteral("status"), status);
    return periods;
#else
    if (!wslInstalled()) {
        status.insert(QStringLiteral("state"), QStringLiteral("not-installed"));
        periods.insert(QStringLiteral("status"), status);
        return periods;
    }
    const auto distros = runningDistros();
    if (distros.isEmpty()) {
        status.insert(QStringLiteral("state"), QStringLiteral("not-running"));
        periods.insert(QStringLiteral("status"), status);
        return periods;
    }
    TokscaleRunner runner(Paths::tokscaleBinary());
    QStringList detected;
    QStringList withData;
    bool any = false;
    for (const auto &distro : distros) {
        const auto list = runProcess(QStringLiteral("wsl.exe"),
                                     {QStringLiteral("-d"), distro, QStringLiteral("bash"), QStringLiteral("-lc"),
                                      QStringLiteral("getent passwd | cut -d: -f6")},
                                     15000);
        const auto homes = QString::fromUtf8(list.stdoutBytes).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (auto home : homes) {
            home = home.trimmed();
            if (home.isEmpty() || home == QLatin1String("/root")) continue;
            const auto linuxHome = home;
            const auto winHome = QStringLiteral("\\\\wsl$\\") + distro
                + QString(linuxHome).replace(QLatin1Char('/'), QLatin1Char('\\'));
            bool hasMarker = false;
            for (const auto &marker : wslDataMarkers()) {
                const auto winPath = QStringLiteral("\\\\wsl$\\") + distro
                    + QString(linuxHome + QLatin1Char('/') + marker).replace(QLatin1Char('/'), QLatin1Char('\\'));
                if (QFileInfo::exists(winPath)) {
                    hasMarker = true;
                    const auto client = wslMarkerClient(marker);
                    if (!client.isEmpty() && !detected.contains(client)) detected.append(client);
                }
            }
            if (!hasMarker) continue;
            any = true;
            auto scanToday = runner.scan(clients, {QStringLiteral("--today"), QStringLiteral("--home"), winHome});
            if (scanToday.ok) {
                const auto period = extractUsageFromTokscale(scanToday.json);
                if (asNumber(period.value(QStringLiteral("totalTokens"))) > 0) {
                    periods.insert(QStringLiteral("today"),
                                   mergePeriods(periods.value(QStringLiteral("today")).toObject(), period));
                    for (auto it = period.value(QStringLiteral("clients")).toObject().begin();
                         it != period.value(QStringLiteral("clients")).toObject().end(); ++it) {
                        if (!withData.contains(it.key())) withData.append(it.key());
                    }
                }
            }
            auto scanMonth = runner.scan(clients, {QStringLiteral("--month"), QStringLiteral("--home"), winHome});
            if (scanMonth.ok)
                periods.insert(QStringLiteral("month"),
                               mergePeriods(periods.value(QStringLiteral("month")).toObject(),
                                            extractUsageFromTokscale(scanMonth.json)));
            auto scanAll = runner.scan(clients, {QStringLiteral("--since"), allTimeSince, QStringLiteral("--home"), winHome});
            if (scanAll.ok)
                periods.insert(QStringLiteral("allTime"),
                               mergePeriods(periods.value(QStringLiteral("allTime")).toObject(),
                                            extractUsageFromTokscale(scanAll.json)));
        }
    }
    status.insert(QStringLiteral("state"), any ? QStringLiteral("active") : QStringLiteral("no-data"));
    status.insert(QStringLiteral("detected"), QJsonArray::fromStringList(detected));
    status.insert(QStringLiteral("withData"), QJsonArray::fromStringList(withData));
    periods.insert(QStringLiteral("status"), status);
    return periods;
#endif
}

} // namespace tmon
