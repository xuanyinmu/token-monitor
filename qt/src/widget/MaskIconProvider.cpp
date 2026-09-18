#include "widget/MaskIconProvider.h"

#include <QPainter>
#include <QSvgRenderer>
#include <QUrl>
#include <QUrlQuery>

namespace tmon {

MaskIconProvider::MaskIconProvider()
    : QQuickImageProvider(QQmlImageProviderBase::Image)
{
}

QImage MaskIconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const int qpos = id.indexOf(QLatin1Char('?'));
    const QString path = qpos >= 0 ? id.left(qpos) : id;
    QColor tint(QStringLiteral("#eef5fb"));
    if (qpos >= 0) {
        const QUrlQuery query(id.mid(qpos + 1));
        const auto c = QColor::fromString(QUrl::fromPercentEncoding(query.queryItemValue(QStringLiteral("c")).toUtf8()));
        if (c.isValid()) tint = c;
    }
    int dim = 32;
    if (requestedSize.width() > 0) dim = std::max(requestedSize.width(), requestedSize.height());
    QImage img(dim, dim, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    const QString qrc = QLatin1Char(':') + (path.startsWith(QLatin1Char('/')) ? path : (QLatin1Char('/') + path));
    QSvgRenderer renderer(qrc);
    if (!renderer.isValid()) {
        if (size) *size = img.size();
        return img;
    }
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&p);
    p.setCompositionMode(QPainter::CompositionMode_SourceIn);
    p.fillRect(img.rect(), tint);
    p.end();
    if (size) *size = img.size();
    return img;
}

} // namespace tmon
