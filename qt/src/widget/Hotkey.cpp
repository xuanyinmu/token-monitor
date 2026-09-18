#include "widget/Hotkey.h"

#include <QGuiApplication>
#include <QKeySequence>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace tmon {

Hotkey::Hotkey(QObject *parent)
    : QObject(parent)
{
    qApp->installNativeEventFilter(this);
}

Hotkey::~Hotkey()
{
    unregister();
    qApp->removeNativeEventFilter(this);
}

void Hotkey::unregister()
{
#ifdef Q_OS_WIN
    if (m_registered) UnregisterHotKey(nullptr, m_id);
#endif
    m_registered = false;
}

bool Hotkey::registerShortcut(const QString &sequence)
{
    unregister();
    if (sequence.trimmed().isEmpty()) return false;
#ifdef Q_OS_WIN
    const auto seq = QKeySequence(sequence);
    if (seq.isEmpty()) return false;
    const auto combo = seq[0];
    const int key = combo.toCombined();
    UINT mods = 0;
    if (key & Qt::ControlModifier) mods |= MOD_CONTROL;
    if (key & Qt::AltModifier) mods |= MOD_ALT;
    if (key & Qt::ShiftModifier) mods |= MOD_SHIFT;
    if (key & Qt::MetaModifier) mods |= MOD_WIN;
    const UINT vk = UINT(key & 0xFF);
    Q_UNUSED(vk);
    UINT mapped = 0;
    switch (key & ~Qt::KeyboardModifierMask) {
    case Qt::Key_M: mapped = 'M'; break;
    default: mapped = UINT(QChar(key & ~Qt::KeyboardModifierMask).toUpper().unicode()); break;
    }
    m_registered = RegisterHotKey(nullptr, m_id, mods, mapped);
    return m_registered;
#else
    Q_UNUSED(sequence);
    return false;
#endif
}

bool Hotkey::nativeEventFilter(const QByteArray &, void *message, qintptr *)
{
#ifdef Q_OS_WIN
    const auto *msg = static_cast<MSG *>(message);
    if (msg && msg->message == WM_HOTKEY && int(msg->wParam) == m_id) {
        emit activated();
        return true;
    }
#else
    Q_UNUSED(message);
#endif
    return false;
}

} // namespace tmon
