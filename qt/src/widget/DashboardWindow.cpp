#include "widget/DashboardWindow.h"

#include "widget/Acrylic.h"
#include "widget/AppState.h"

#include <QQmlContext>
#include <QQuickWindow>

namespace tmon {

DashboardWindow::DashboardWindow(AppState *state, QObject *parent)
    : QObject(parent)
    , m_state(state)
{
}

void DashboardWindow::show()
{
    if (!m_loaded) {
        m_engine.rootContext()->setContextProperty(QStringLiteral("app"), m_state);
        m_engine.load(QUrl(QStringLiteral("qrc:/qt/qml/TokenMonitor/Dashboard.qml")));
        m_loaded = true;
    }
    for (auto *obj : m_engine.rootObjects()) {
        if (auto *w = qobject_cast<QQuickWindow *>(obj)) {
            applyAcrylic(w, QStringLiteral("acrylic"), 68, 32);
            w->show();
            w->raise();
        }
    }
}

void DashboardWindow::hide()
{
    for (auto *obj : m_engine.rootObjects()) {
        if (auto *w = qobject_cast<QQuickWindow *>(obj)) w->hide();
    }
}

} // namespace tmon
