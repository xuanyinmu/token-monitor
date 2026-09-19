#pragma once

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QObject>
#include <QWindow>

class QVariantAnimation;
class QTimer;

namespace tmon {

class WidgetWindow : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit WidgetWindow(QWindow *window, QObject *parent = nullptr);
    ~WidgetWindow() override;
    Q_INVOKABLE void applyChrome(const QString &behavior, const QString &backdrop, int opacity, int blur, bool keepAboveTaskbar, bool hideAppIcon = false);
    Q_INVOKABLE void startMove();
    Q_INVOKABLE void startResize(Qt::Edges edges);
    Q_INVOKABLE void minimize();
    Q_INVOKABLE void closeWindow();
    Q_INVOKABLE void hideWindow();
    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void dockTopEdge(bool docked);
    Q_INVOKABLE void setTopEdgeEnabled(bool on);
    QWindow *window() const { return m_window; }
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void pollCursor();
    void animateWindowY(int targetY, bool hiding);
    void stopYAnimation();
    QWindow *m_window = nullptr;
    QTimer *m_edgeTimer = nullptr;
    QTimer *m_dockDebounce = nullptr;
    QVariantAnimation *m_yAnimation = nullptr;
    bool m_topHidden = false;
    bool m_topEdgeEnabled = false;
    int m_restY = 0;
    bool m_filterInstalled = false;
};

} // namespace tmon
