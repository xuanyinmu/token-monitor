#pragma once

#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace tmon {

struct TokscaleScan {
    bool ok = false;
    QString error;
    QJsonValue json;
    QString grouping;
};

class TokscaleRunner {
public:
    explicit TokscaleRunner(QString binary);
    TokscaleScan scan(const QStringList &clients, const QStringList &extraArgs) const;
    TokscaleScan scanToday(const QStringList &clients) const;
    TokscaleScan scanMonth(const QStringList &clients) const;
    TokscaleScan scanSince(const QStringList &clients, const QString &since) const;
    QString binary() const { return m_binary; }

private:
    QString m_binary;
    mutable QString m_grouping = QStringLiteral("client,workspace,session,model");
};

} // namespace tmon
