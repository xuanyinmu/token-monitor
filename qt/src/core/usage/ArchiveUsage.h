#pragma once

#include <QJsonObject>
#include <QStringList>

namespace tmon {

// Electron main-process display compositions (clientUsageArchive.js /
// sessionUsageArchive.js, applied as `transformUsage` on the local record):
// clients that were untracked and sessions that aged out of live scans are
// folded back into today/month/allTime so totals keep counting them. allTime
// applies unconditionally; today/month only while the capture window matches.

void applyArchivedClientUsageToRecord(QJsonObject &record, const QJsonObject &archive,
                                      const QStringList &activeClients);

void applySessionUsageArchiveToRecord(QJsonObject &record, const QJsonObject &archive);

} // namespace tmon
