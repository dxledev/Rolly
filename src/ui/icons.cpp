#include "icons.h"

#include <QPainter>
#include <QPainterPath>

namespace rolly {
namespace {

// Paths are drawn on a 24 x 24 grid.
void draw(QPainter &p, Glyph glyph) {
    QPainterPath path;
    switch (glyph) {
    case Glyph::Open:
        path.moveTo(3, 18); path.lineTo(3, 6); path.lineTo(9, 6); path.lineTo(11, 8.5); path.lineTo(20, 8.5); path.lineTo(20, 11);
        path.moveTo(3, 18); path.lineTo(6, 11); path.lineTo(22, 11); path.lineTo(19, 18); path.closeSubpath();
        break;
    case Glyph::New:
        path.addRoundedRect(QRectF(4, 4, 16, 16), 3, 3);
        path.moveTo(12, 8.5); path.lineTo(12, 15.5); path.moveTo(8.5, 12); path.lineTo(15.5, 12);
        break;
    case Glyph::Extract:
        path.moveTo(4, 14); path.lineTo(4, 20); path.lineTo(20, 20); path.lineTo(20, 14);
        path.moveTo(12, 3.5); path.lineTo(12, 15); path.moveTo(7.5, 10.5); path.lineTo(12, 15); path.lineTo(16.5, 10.5);
        break;
    case Glyph::Add:
        path.moveTo(12, 5); path.lineTo(12, 19); path.moveTo(5, 12); path.lineTo(19, 12);
        break;
    case Glyph::Remove:
        path.moveTo(4.5, 7); path.lineTo(19.5, 7); path.moveTo(9, 7); path.lineTo(9, 4.5); path.lineTo(15, 4.5); path.lineTo(15, 7);
        path.moveTo(6.5, 7); path.lineTo(7.5, 20); path.lineTo(16.5, 20); path.lineTo(17.5, 7);
        break;
    case Glyph::Up:
        path.moveTo(12, 19); path.lineTo(12, 5); path.moveTo(6, 11); path.lineTo(12, 5); path.lineTo(18, 11);
        break;
    case Glyph::Info:
        path.addEllipse(QRectF(3.5, 3.5, 17, 17));
        path.moveTo(12, 11); path.lineTo(12, 16.5); path.moveTo(12, 7.6); path.lineTo(12, 7.7);
        break;
    case Glyph::Folder:
        path.moveTo(3, 18.5); path.lineTo(3, 5.5); path.lineTo(9.5, 5.5); path.lineTo(11.5, 8); path.lineTo(21, 8); path.lineTo(21, 18.5); path.closeSubpath();
        break;
    case Glyph::File:
        path.moveTo(6, 3.5); path.lineTo(14, 3.5); path.lineTo(19, 8.5); path.lineTo(19, 20.5); path.lineTo(6, 20.5); path.closeSubpath();
        path.moveTo(14, 3.5); path.lineTo(14, 8.5); path.lineTo(19, 8.5);
        break;
    case Glyph::Link:
        path.moveTo(10, 14); path.lineTo(14, 10);
        path.moveTo(8.5, 11.5); path.lineTo(6.5, 13.5); path.quadTo(4.5, 16, 7, 17.5); path.quadTo(9, 19, 10.5, 17.5); path.lineTo(12, 16);
        path.moveTo(15.5, 12.5); path.lineTo(17.5, 10.5); path.quadTo(19.5, 8, 17, 6.5); path.quadTo(15, 5, 13.5, 6.5); path.lineTo(12, 8);
        break;
    case Glyph::Lock:
        path.addRoundedRect(QRectF(5, 11, 14, 9.5), 2, 2);
        path.moveTo(8, 11); path.lineTo(8, 8); path.quadTo(8, 4, 12, 4); path.quadTo(16, 4, 16, 8); path.lineTo(16, 11);
        break;
    case Glyph::Archive:
        path.addRoundedRect(QRectF(3, 4, 18, 5), 1.5, 1.5);
        path.moveTo(4.5, 9); path.lineTo(4.5, 19.5); path.lineTo(19.5, 19.5); path.lineTo(19.5, 9);
        path.moveTo(10, 13); path.lineTo(14, 13);
        break;
    case Glyph::Close:
        path.moveTo(6, 6); path.lineTo(18, 18); path.moveTo(18, 6); path.lineTo(6, 18);
        break;
    }
    p.drawPath(path);
}

}

QPixmap glyphPixmap(Glyph glyph, const QColor &color, int size, qreal ratio) {
    QPixmap pixmap(QSize(size, size) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.scale(size / 24.0, size / 24.0);
    QPen pen(color, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    draw(p, glyph);
    return pixmap;
}

QIcon glyphIcon(Glyph glyph, const QColor &color, int size) {
    QIcon icon;
    icon.addPixmap(glyphPixmap(glyph, color, size, 1));
    icon.addPixmap(glyphPixmap(glyph, color, size, 2));
    return icon;
}

}
