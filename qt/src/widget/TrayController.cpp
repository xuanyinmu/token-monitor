#include "widget/TrayController.h"

#include <QAction>
#include <QIcon>
#include <QMenu>

namespace tmon {

TrayController::TrayController(QObject *parent)
    : QObject(parent)
    , m_icon(new QSystemTrayIcon(this))
{
    m_icon->setIcon(QIcon(QStringLiteral(":/icons/icons/token-monitor.svg")));
    auto *menu = new QMenu();
    menu->addAction(QStringLiteral("Home"), this, &TrayController::showHome);
    menu->addAction(QStringLiteral("Limits"), this, &TrayController::showLimits);
    menu->addAction(QStringLiteral("Settings"), this, &TrayController::showSettings);
    menu->addSeparator();
    menu->addAction(QStringLiteral("Quit"), this, &TrayController::quitRequested);
    m_icon->setContextMenu(menu);
    connect(m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) emit activated();
    });
}

void TrayController::setVisible(bool on)
{
    m_icon->setVisible(on);
}

void TrayController::setTooltip(const QString &text)
{
    m_icon->setToolTip(text);
}

void TrayController::showMessage(const QString &title, const QString &body)
{
    m_icon->showMessage(title, body);
}

} // namespace tmon
