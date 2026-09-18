#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace tmon {

struct SourceRoot {
    QString client;
    QString checkId;
    QString dir;
    QString sourcePath;
    bool optional = false;
    bool custom = false;
};

QVector<SourceRoot> clientSourceRoots(const QString &clientsCsv);
QStringList watchDirsForClients(const QString &clientsCsv);
QStringList selfSyncedClients();
QStringList wslDataMarkers();
QString wslMarkerClient(const QString &marker);

} // namespace tmon
