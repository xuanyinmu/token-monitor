#pragma once

#include <QString>

class QWindow;

namespace tmon {

void applyAcrylic(QWindow *window, const QString &mode, int glassOpacity, int glassBlur);
void applyRoundedCorners(QWindow *window);
void setAlwaysOnTop(QWindow *window, bool on, bool keepAboveTaskbar);

} // namespace tmon
