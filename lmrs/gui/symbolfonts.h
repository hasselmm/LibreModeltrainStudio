#ifndef LMRS_GUI_SYMBOLFONTS_H
#define LMRS_GUI_SYMBOLFONTS_H

#include <QtGlobal>

class QColor;
class QIcon;
class QString;

namespace lmrs::gui::symbolfonts {

enum class Style { Normal, Inverted, Selected };

QIcon icon(QString key, QString fontName, Style style, qreal scale, QString text);
QIcon icon(QString key, QString fontName, QColor color, Style style, qreal scale, QString text);

} // namespace lmrs::gui::symbolfonts

#endif // LMRS_GUI_SYMBOLFONTS_H
