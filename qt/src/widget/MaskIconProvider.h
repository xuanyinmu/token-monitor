#pragma once

#include <QQuickImageProvider>

namespace tmon {

class MaskIconProvider : public QQuickImageProvider {
public:
    MaskIconProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

} // namespace tmon
