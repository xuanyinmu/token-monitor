#pragma once

#include <QString>
#include <QStringList>

namespace tmon {

void maybeSelfSync(const QStringList &clients, const QString &tokscaleBinary, bool sourceOnly = false);

} // namespace tmon
