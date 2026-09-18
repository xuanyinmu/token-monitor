#include "core/usage/UsageWatcher.h"

#include "core/tmon.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSet>
#include <QThread>
#include <QTimer>

namespace tmon {
namespace {

constexpr int kWatchMaxDirs = 500;
constexpr int kWatchMaxDepth = 4;

void collectDirs(const QString &root, int depth, QStringList &out, QSet<QString> &seen)
{
    if (depth > kWatchMaxDepth || out.size() >= kWatchMaxDirs) return;
    const auto canonical = QDir(root).absolutePath();
    if (seen.contains(canonical) || !QFileInfo::exists(canonical)) return;
    seen.insert(canonical);
    out.append(canonical);
    if (depth == kWatchMaxDepth) return;
    QDir dir(canonical);
    const auto kids = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto &kid : kids) {
        if (out.size() >= kWatchMaxDirs) break;
        collectDirs(kid.absoluteFilePath(), depth + 1, out, seen);
    }
}

} // namespace

class WatcherWorker : public QObject {
    Q_OBJECT
public:
    explicit WatcherWorker(QObject *parent = nullptr)
        : QObject(parent)
        , m_watcher(new QFileSystemWatcher(this))
        , m_debounce(new QTimer(this))
    {
        m_debounce->setSingleShot(true);
        m_debounce->setInterval(kWatchDebounceMs);
        connect(m_debounce, &QTimer::timeout, this, [this]() {
            const auto paths = m_pending;
            m_pending.clear();
            emit dirty(paths);
        });
        connect(m_watcher, &QFileSystemWatcher::directoryChanged, this, &WatcherWorker::arm);
        connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &WatcherWorker::arm);
    }

public slots:
    void setRoots(const QStringList &dirs)
    {
        const auto current = m_watcher->directories() + m_watcher->files();
        if (!current.isEmpty()) m_watcher->removePaths(current);
        QStringList existing;
        QSet<QString> seen;
        for (const auto &dir : dirs) collectDirs(dir, 0, existing, seen);
        if (!existing.isEmpty()) m_watcher->addPaths(existing);
    }

    void stop()
    {
        m_debounce->stop();
        const auto current = m_watcher->directories() + m_watcher->files();
        if (!current.isEmpty()) m_watcher->removePaths(current);
        m_pending.clear();
    }

signals:
    void dirty(const QStringList &paths);

private:
    void arm(const QString &path)
    {
        if (!m_pending.contains(path)) m_pending.append(path);
        m_debounce->start();
    }

    QFileSystemWatcher *m_watcher = nullptr;
    QTimer *m_debounce = nullptr;
    QStringList m_pending;
};

UsageWatcher::UsageWatcher(QObject *parent)
    : QObject(parent)
    , m_thread(new QThread(this))
{
    auto *worker = new WatcherWorker();
    worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(this, &UsageWatcher::requestSetRoots, worker, &WatcherWorker::setRoots, Qt::QueuedConnection);
    connect(this, &UsageWatcher::requestStop, worker, &WatcherWorker::stop, Qt::QueuedConnection);
    connect(worker, &WatcherWorker::dirty, this, &UsageWatcher::dirty, Qt::QueuedConnection);
    m_thread->start();
}

UsageWatcher::~UsageWatcher()
{
    emit requestStop();
    m_thread->quit();
    m_thread->wait(2000);
}

void UsageWatcher::setRoots(const QStringList &dirs)
{
    emit requestSetRoots(dirs);
}

void UsageWatcher::stop()
{
    emit requestStop();
}

} // namespace tmon

#include "UsageWatcher.moc"
