#include "core/io/HashKey.h"

#include <QCryptographicHash>

namespace tmon {
namespace {

QString digest(const QStringList &parts)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (const auto &part : parts) {
        hash.addData(part.toUtf8());
        hash.addData(QByteArray(1, '\0'));
    }
    return QStringLiteral("sha256:") + QString::fromLatin1(hash.result().toHex());
}

} // namespace

QString hashKey(const QStringList &parts)
{
    return digest(parts);
}

QString hashKey(const QString &a, const QString &b, const QString &c)
{
    QStringList parts{a};
    if (!b.isEmpty()) parts.append(b);
    if (!c.isEmpty()) parts.append(c);
    return digest(parts);
}

} // namespace tmon
