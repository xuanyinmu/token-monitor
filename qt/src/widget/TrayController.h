#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <QVariantMap>

class QMenu;

namespace tmon {

// Mirrors Electron tray.js buildTrayMenuTemplate: refresh, an "open view"
// submenu over every breakdown, a tray-display submenu, a window-presentation
// submenu, then version/settings/quit. AppState builds the localized spec.
class TrayController : public QObject {
    Q_OBJECT
public:
    explicit TrayController(QObject *parent = nullptr);
    Q_INVOKABLE void setVisible(bool on);
    Q_INVOKABLE void setTooltip(const QString &text);
    Q_INVOKABLE void showMessage(const QString &title, const QString &body);
    // spec: { refreshLabel, refreshEnabled, openViewLabel,
    //         views: [{id,label,enabled}], contentLabel,
    //         contentOptions: [{value,label}], contentCurrent,
    //         presentationLabel, presentationOptions: [{value,label}],
    //         presentationCurrent, versionLabel, settingsLabel, quitLabel }
    Q_INVOKABLE void rebuildMenu(const QVariantMap &spec);

signals:
    void activated();
    void showHome();
    void showLimits();
    void showSettings();
    void quitRequested();
    void refreshRequested();
    void openViewRequested(const QString &view);
    void trayContentRequested(const QString &content);
    void presentationRequested(const QString &presentation);

private:
    QSystemTrayIcon *m_icon = nullptr;
    QMenu *m_menu = nullptr;
};

} // namespace tmon
