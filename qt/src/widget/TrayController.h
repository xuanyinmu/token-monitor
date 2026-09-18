#pragma once

#include <QObject>
#include <QSystemTrayIcon>

namespace tmon {

class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(QObject *parent = nullptr);
    Q_INVOKABLE void setVisible(bool on);
    Q_INVOKABLE void setTooltip(const QString &text);
    Q_INVOKABLE void showMessage(const QString &title, const QString &body);

signals:
    void activated();
    void showHome();
    void showLimits();
    void showSettings();
    void quitRequested();

private:
    QSystemTrayIcon *m_icon = nullptr;
};

} // namespace tmon
