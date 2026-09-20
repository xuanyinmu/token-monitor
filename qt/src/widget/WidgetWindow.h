#pragma once

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QObject>
#include <QPointer>
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
    // Introspection for the self-test: a completed animation must leave no handle
    // behind. Defined in the .cpp because comparing a QPointer needs the complete
    // pointee type, which this header only forward-declares.
    bool hasYAnimation() const;
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void pollCursor();
    void animateWindowY(int targetY, bool hiding);
    void stopYAnimation();
    QPointer<QWindow> m_window;
    QTimer *m_edgeTimer = nullptr;
    QTimer *m_dockDebounce = nullptr;
    // Must be a QPointer. The animation is started with DeleteWhenStopped, so Qt
    // frees it when it finishes; a raw pointer then dangles and the next
    // stopYAnimation() called stop() on freed memory. That is the crash 0.57 shipped
    // (0xc0000005 inside QAbstractAnimation::stop(), faulting module Qt6Core.dll):
    // dock, move the pointer onto the 6px peek, and the reveal stopped the animation
    // that had already been freed.
    QPointer<QVariantAnimation> m_yAnimation;
    bool m_topHidden = false;
    bool m_topEdgeEnabled = false;
    int m_restY = 0;
    bool m_filterInstalled = false;
};

} // namespace tmon
