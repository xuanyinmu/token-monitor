#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>

namespace tmon {

class Hotkey : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit Hotkey(QObject *parent = nullptr);
    ~Hotkey() override;
    Q_INVOKABLE bool registerShortcut(const QString &sequence);
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

signals:
    void activated();

private:
    void unregister();
    int m_id = 1;
    bool m_registered = false;
};

} // namespace tmon
