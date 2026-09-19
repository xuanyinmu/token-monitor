#include "widget/TrayController.h"

#include <QAction>
#include <QIcon>
#include <QMenu>

namespace tmon {

TrayController::TrayController(QObject *parent)
    : QObject(parent)
    , m_icon(new QSystemTrayIcon(this))
    , m_menu(new QMenu())
{
    m_icon->setIcon(QIcon(QStringLiteral(":/icons/icons/token-monitor.svg")));
    m_icon->setContextMenu(m_menu);
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

void TrayController::rebuildMenu(const QVariantMap &spec)
{
    m_menu->clear();
    auto *refresh = m_menu->addAction(spec.value(QStringLiteral("refreshLabel")).toString());
    refresh->setEnabled(spec.value(QStringLiteral("refreshEnabled"), true).toBool());
    connect(refresh, &QAction::triggered, this, &TrayController::refreshRequested);

    auto *openView = m_menu->addMenu(spec.value(QStringLiteral("openViewLabel")).toString());
    const auto views = spec.value(QStringLiteral("views")).toList();
    for (const auto &v : views) {
        const auto map = v.toMap();
        auto *action = openView->addAction(map.value(QStringLiteral("label")).toString());
        action->setEnabled(map.value(QStringLiteral("enabled"), true).toBool());
        const auto id = map.value(QStringLiteral("id")).toString();
        connect(action, &QAction::triggered, this, [this, id]() { emit openViewRequested(id); });
    }

    m_menu->addSeparator();
    auto *contentMenu = m_menu->addMenu(spec.value(QStringLiteral("contentLabel")).toString());
    const auto contentOptions = spec.value(QStringLiteral("contentOptions")).toList();
    const auto contentCurrent = spec.value(QStringLiteral("contentCurrent")).toString();
    for (const auto &o : contentOptions) {
        const auto map = o.toMap();
        const auto value = map.value(QStringLiteral("value")).toString();
        auto *action = contentMenu->addAction(map.value(QStringLiteral("label")).toString());
        action->setCheckable(true);
        action->setChecked(value == contentCurrent);
        connect(action, &QAction::triggered, this, [this, value]() { emit trayContentRequested(value); });
    }

    auto *presentationMenu = m_menu->addMenu(spec.value(QStringLiteral("presentationLabel")).toString());
    const auto presentationOptions = spec.value(QStringLiteral("presentationOptions")).toList();
    const auto presentationCurrent = spec.value(QStringLiteral("presentationCurrent")).toString();
    for (const auto &o : presentationOptions) {
        const auto map = o.toMap();
        const auto value = map.value(QStringLiteral("value")).toString();
        auto *action = presentationMenu->addAction(map.value(QStringLiteral("label")).toString());
        action->setCheckable(true);
        action->setChecked(value == presentationCurrent);
        connect(action, &QAction::triggered, this, [this, value]() { emit presentationRequested(value); });
    }

    m_menu->addSeparator();
    if (!spec.value(QStringLiteral("versionLabel")).toString().isEmpty()) {
        auto *version = m_menu->addAction(spec.value(QStringLiteral("versionLabel")).toString());
        version->setEnabled(false);
    }
    m_menu->addAction(spec.value(QStringLiteral("settingsLabel")).toString(), this, [this]() { emit showSettings(); });
    m_menu->addAction(spec.value(QStringLiteral("quitLabel")).toString(), this, &TrayController::quitRequested);
}

} // namespace tmon
