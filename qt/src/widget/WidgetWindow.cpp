#include "widget/WidgetWindow.h"

#include "widget/Acrylic.h"

#include <QColor>
#include <QCoreApplication>
#include <QCursor>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace tmon {

WidgetWindow::WidgetWindow(QWindow *window, QObject *parent)
    : QObject(parent)
    , m_window(window)
    , m_edgeTimer(new QTimer(this))
{
    m_edgeTimer->setInterval(80);
    connect(m_edgeTimer, &QTimer::timeout, this, &WidgetWindow::pollCursor);
}

WidgetWindow::~WidgetWindow()
{
    if (m_filterInstalled) {
        if (auto *core = QCoreApplication::instance())
            core->removeNativeEventFilter(this);
    }
}

bool WidgetWindow::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (!m_window || eventType != "windows_generic_MSG") return false;
    const auto *msg = static_cast<MSG *>(message);
    if (msg->hwnd != reinterpret_cast<HWND>(m_window->winId())) return false;
    // WS_THICKFRAME otherwise keeps a DWM caption + left/right resize frame
    // outside the QML client, so the top strip is a different color and the
    // content looks shifted right.
    if (msg->message == WM_NCCALCSIZE && msg->wParam) {
        *result = 0;
        return true;
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return false;
}

void WidgetWindow::applyChrome(const QString &behavior, const QString &backdrop, int opacity, int blur, bool keepAboveTaskbar)
{
    if (!m_window) return;
    m_window->setFlag(Qt::FramelessWindowHint, true);
    if (auto *quick = qobject_cast<QQuickWindow *>(m_window))
        quick->setColor(QColor(0x30, 0x34, 0x38));
    if (!m_filterInstalled) {
        if (auto *core = QCoreApplication::instance()) {
            core->installNativeEventFilter(this);
            m_filterInstalled = true;
        }
    }
    const bool floating = behavior == QLatin1String("floating");
    const bool desktop = behavior == QLatin1String("desktop");
    setAlwaysOnTop(m_window, floating, keepAboveTaskbar);
    m_window->setFlag(Qt::WindowTransparentForInput, desktop);
    applyAcrylic(m_window, backdrop, opacity, blur);
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    const LONG style = GetWindowLongW(hwnd, GWL_STYLE);
    SetWindowLongW(hwnd, GWL_STYLE, style | WS_THICKFRAME | WS_MINIMIZEBOX);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    const QSize client = m_window->size();
    if (client.width() > 0 && client.height() > 0)
        m_window->resize(client);
#endif
}

void WidgetWindow::startMove()
{
    if (m_window) m_window->startSystemMove();
}

void WidgetWindow::startResize(Qt::Edges edges)
{
    if (m_window) m_window->startSystemResize(edges);
}

void WidgetWindow::minimize()
{
    if (m_window) m_window->setVisibility(QWindow::Minimized);
}

void WidgetWindow::closeWindow()
{
    if (m_window) m_window->close();
}

void WidgetWindow::hideWindow()
{
    if (m_window) m_window->hide();
}

void WidgetWindow::showWindow()
{
    if (!m_window) return;
    if (m_topHidden) {
        m_window->setY(m_restY);
        m_topHidden = false;
    }
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void WidgetWindow::dockTopEdge(bool docked)
{
    if (!m_window) return;
    if (!docked) {
        if (m_topHidden) {
            m_window->setY(m_restY);
            m_topHidden = false;
        }
        return;
    }
    if (!m_topHidden) m_restY = m_window->y();
    const int hideY = 4 - m_window->height();
    m_window->setY(hideY);
    m_topHidden = true;
}

void WidgetWindow::setTopEdgeEnabled(bool on)
{
    m_topEdgeEnabled = on;
    if (!on) {
        m_edgeTimer->stop();
        dockTopEdge(false);
        return;
    }
    if (m_window) m_restY = m_window->y();
    m_edgeTimer->start();
}

void WidgetWindow::pollCursor()
{
    if (!m_topEdgeEnabled || !m_window || !m_window->isVisible()) return;
    const auto pos = QCursor::pos();
    const auto geo = m_window->geometry();
    const int screenTop = m_window->screen() ? m_window->screen()->geometry().top() : 0;
    if (m_topHidden) {
        if (pos.y() <= screenTop + 6 && pos.x() >= geo.x() && pos.x() <= geo.x() + geo.width()) {
            m_window->setY(m_restY);
            m_topHidden = false;
        }
        return;
    }
    if (pos.y() > geo.bottom() + 24 && geo.y() <= screenTop + 8)
        dockTopEdge(true);
}

} // namespace tmon
