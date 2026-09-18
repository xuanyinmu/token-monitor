#pragma once

#include <QObject>
#include <QStringList>
#include <QThread>

namespace tmon {

class UsageWatcher : public QObject {
    Q_OBJECT
public:
    explicit UsageWatcher(QObject *parent = nullptr);
    ~UsageWatcher() override;
    void setRoots(const QStringList &dirs);
    void stop();

signals:
    void dirty(const QStringList &paths);
    void requestSetRoots(const QStringList &dirs);
    void requestStop();

private:
    QThread *m_thread = nullptr;
};

} // namespace tmon
