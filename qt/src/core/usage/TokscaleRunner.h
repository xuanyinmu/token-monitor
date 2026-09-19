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
    // Electron tokscaleCommand injects settings.customScanPaths as the
    // TOKSCALE_EXTRA_DIRS env (expanded to each client's scan ids); an empty
    // value runs the binary with the inherited environment.
    explicit TokscaleRunner(QString binary, QString extraDirsEnv);
    TokscaleScan scan(const QStringList &clients, const QStringList &extraArgs) const;
    TokscaleScan scanToday(const QStringList &clients) const;
    TokscaleScan scanMonth(const QStringList &clients) const;
    TokscaleScan scanSince(const QStringList &clients, const QString &since) const;
    // tokscale graph: per-day contributions with active time and per-client /
    // per-model attribution — the source of Electron's daily history.
    TokscaleScan scanGraph(const QStringList &clients) const;
    QString binary() const { return m_binary; }

private:
    QString m_binary;
    QString m_extraDirsEnv;
    mutable QString m_grouping = QStringLiteral("client,workspace,session,model");
};

} // namespace tmon
