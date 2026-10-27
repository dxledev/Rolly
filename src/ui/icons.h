#pragma once

#include <QColor>
#include <QIcon>

namespace rolly {

enum class Glyph { Open, New, Extract, Add, Remove, Up, Info, Folder, File, Link, Lock, Archive, Close };

// Every glyph is a stroked path in one flat color, so it follows the theme.
QPixmap glyphPixmap(Glyph glyph, const QColor &color, int size, qreal ratio = 1);
QIcon glyphIcon(Glyph glyph, const QColor &color, int size = 18);

}
