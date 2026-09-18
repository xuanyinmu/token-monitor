#include "core/usage/SelfSync.h"

#include "core/io/Paths.h"
#include "core/process/Subprocess.h"
#include "core/tmon.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>

namespace tmon {
namespace {

QHash<QString, qint64> g_lastSync;

bool due(const QString &client, int minMs)
{
    const auto now = QDateTime::currentMSecsSinceEpoch();
    if (now - g_lastSync.value(client, 0) < minMs) return false;
    g_lastSync.insert(client, now);
    return true;
}

bool dirExists(const QString &path)
{
    return QFileInfo(path).isDir();
}

} // namespace

void maybeSelfSync(const QStringList &clients, const QString &tokscaleBinary, bool sourceOnly)
{
    const int minMs = sourceOnly ? 10'000 : kSelfSyncMinIntervalMs;
    if (clients.contains(QStringLiteral("antigravity")) && due(QStringLiteral("antigravity"), minMs)) {
        const QString gemini = Paths::envOr(QStringLiteral("GEMINI_CLI_HOME"),
                                            QDir(Paths::homeDir()).filePath(QStringLiteral(".gemini")));
        const bool present = dirExists(QDir(gemini).filePath(QStringLiteral("antigravity")))
            || dirExists(QDir(gemini).filePath(QStringLiteral("antigravity-ide")))
            || dirExists(QDir(gemini).filePath(QStringLiteral("antigravity-backup")));
        if (present) {
            runProcess(tokscaleBinary, {QStringLiteral("antigravity"), QStringLiteral("sync")}, 120000);
        }
    }
    if (clients.contains(QStringLiteral("cursor")) && due(QStringLiteral("cursor"), minMs)) {
        const QString cursor = QDir(Paths::homeDir()).filePath(QStringLiteral(".cursor"));
        if (dirExists(cursor) || QFileInfo::exists(QDir(Paths::appDataRoaming()).filePath(QStringLiteral("Cursor")))) {
            runProcess(tokscaleBinary, {QStringLiteral("cursor"), QStringLiteral("sync")}, 120000);
        }
    }
}

} // namespace tmon
