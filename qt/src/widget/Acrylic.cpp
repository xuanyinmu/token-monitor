#include "widget/Acrylic.h"

#include <QWindow>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <dwmapi.h>
#  pragma comment(lib, "dwmapi.lib")
#  pragma comment(lib, "user32.lib")
#  pragma comment(lib, "gdi32.lib")

namespace {

constexpr DWORD WCA_ACCENT_POLICY = 19;
constexpr DWORD ACCENT_ENABLE_ACRYLICBLURBEHIND = 4;
constexpr DWORD kDwmBorderColor = 34;
constexpr DWORD kDwmColorNone = 0xFFFFFFFE;

struct ACCENTPOLICY {
    int AccentState;
    int AccentFlags;
    unsigned GradientColor;
    int AnimationId;
};
struct WINCOMPATTRDATA {
    unsigned Attrib;
    void *pvData;
    unsigned cbData;
};

using SetWindowCompositionAttributeFn = BOOL(WINAPI *)(HWND, WINCOMPATTRDATA *);

HWND hwndOf(QWindow *window)
{
    return reinterpret_cast<HWND>(window->winId());
}

void applyAccent(HWND hwnd, unsigned argb)
{
    auto fn = reinterpret_cast<SetWindowCompositionAttributeFn>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
    if (!fn) return;
    ACCENTPOLICY policy{int(ACCENT_ENABLE_ACRYLICBLURBEHIND), 0, argb, 0};
    WINCOMPATTRDATA data{WCA_ACCENT_POLICY, &policy, sizeof(policy)};
    DWM_BLURBEHIND bb{};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION | DWM_BB_TRANSITIONONMAXIMIZED;
    bb.fEnable = TRUE;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    DwmEnableBlurBehindWindow(hwnd, &bb);
    if (bb.hRgnBlur) DeleteObject(bb.hRgnBlur);
    MARGINS margins{-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    fn(hwnd, &data);
}

} // namespace
#endif

namespace tmon {

void applyRoundedCorners(QWindow *window)
{
#ifdef Q_OS_WIN
    if (!window) return;
    DWORD pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwndOf(window), DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
    DWORD none = kDwmColorNone;
    DwmSetWindowAttribute(hwndOf(window), kDwmBorderColor, &none, sizeof(none));
    // Match the QML Theme.bg so a leftover caption cannot paint light gray.
    const COLORREF caption = RGB(0x30, 0x34, 0x38);
    DwmSetWindowAttribute(hwndOf(window), 35, &caption, sizeof(caption));
#else
    Q_UNUSED(window);
#endif
}

void applyAcrylic(QWindow *window, const QString &mode, int glassOpacity, int glassBlur)
{
#ifdef Q_OS_WIN
    if (!window) return;
    const HWND hwnd = hwndOf(window);
    applyRoundedCorners(window);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark));
    SetClassLongPtrW(hwnd, -16, GetClassLongPtrW(hwnd, -16) | CS_DROPSHADOW);
    if (mode == QLatin1String("off")) {
        DWORD type = 0; // DWMSBT_NONE
        DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
        MARGINS none{0, 0, 0, 0};
        DwmExtendFrameIntoClientArea(hwnd, &none);
        return;
    }
    if (mode == QLatin1String("accent")) {
        MARGINS margins{-1, -1, -1, -1};
        DwmExtendFrameIntoClientArea(hwnd, &margins);
        const int alpha = qBound(1, 255 - int(glassOpacity * 2.2) - glassBlur / 4, 180);
        const unsigned argb = (unsigned(alpha) << 24) | 0x00232323;
        applyAccent(hwnd, argb);
        return;
    }
    DWORD type = DWMSBT_TRANSIENTWINDOW;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
    MARGINS margins{-1, -1, -1, -1};
    DwmExtendFrameIntoClientArea(hwnd, &margins);
#else
    Q_UNUSED(window); Q_UNUSED(mode); Q_UNUSED(glassOpacity); Q_UNUSED(glassBlur);
#endif
}

void setAlwaysOnTop(QWindow *window, bool on, bool keepAboveTaskbar)
{
#ifdef Q_OS_WIN
    if (!window) return;
    const HWND hwnd = hwndOf(window);
    HWND insert = on ? HWND_TOPMOST : HWND_NOTOPMOST;
    if (on && keepAboveTaskbar) insert = HWND_TOPMOST;
    SetWindowPos(hwnd, insert, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (on && keepAboveTaskbar) {
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
#else
    window->setFlag(Qt::WindowStaysOnTopHint, on);
    Q_UNUSED(keepAboveTaskbar);
#endif
}

} // namespace tmon
