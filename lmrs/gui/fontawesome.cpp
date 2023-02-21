#include "fontawesome.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/userliterals.h>

#include <QFontDatabase>
#include <QIcon>

extern int qInitResources_fontawesome();

namespace lmrs::gui::fontawesome {

namespace {

auto initResources()
{
    static const auto resourceId = qInitResources_fontawesome();
    return resourceId;
}

constexpr auto s_keyPrefixSolid = "lmrs/gui/fontawesome/solid/"_L1;
constexpr auto s_keyPrefixRegular = "lmrs/gui/fontawesome/regular/"_L1;
constexpr auto s_keyInfixStyleNormal = "normal/"_L1;
constexpr auto s_keyInfixStyleInverted = "inverted/"_L1;
constexpr auto s_keyInfixStyleSelected = "selected/"_L1;

QString styleInfix(Style style)
{
    switch (style) {
    case Style::Normal:
        return s_keyInfixStyleNormal;
    case Style::Inverted:
        return s_keyInfixStyleInverted;
    case Style::Selected:
        return s_keyInfixStyleSelected;
    }

    return QString::number(core::value(style)) + ':'_L1;
}

template<typename Icon>
auto key(QString prefix, Icon icon, QColor color, Style style)
{
    auto key = prefix + styleInfix(style) + QString::number(icon->unicode(), 16);

    if (color.isValid())
        key += '/'_L1 + QString::number(color.rgba(), 16).rightJustified(8, '0'_L1);

    return key;
}

} // namespace

QString solidFontFamily()
{
    initResources();
    static const auto fileName = ":/taschenorakel.de/lmrs/gui/fontawesome/otfs/Font Awesome 6 Free-Solid-900.otf"_L1;
    static const auto fontId = QFontDatabase::addApplicationFont(fileName);
    static const auto familyNames = QFontDatabase::applicationFontFamilies(fontId);
    return familyNames.first();
}

QString regularFontFamily()
{
    initResources();
    static const auto fileName = ":/taschenorakel.de/lmrs/gui/fontawesome/otfs/Font Awesome 6 Free-Regular-400.otf"_L1;
    static const auto fontId = QFontDatabase::addApplicationFont(fileName);
    static const auto familyNames = QFontDatabase::applicationFontFamilies(fontId);
    return familyNames.first();
}

QIcon icon(SolidIcon icon, Style style)
{
    return fontawesome::icon(icon, QColor{}, style);
}

QIcon icon(SolidIcon icon, QColor color, Style style)
{
    return symbolfonts::icon(key(s_keyPrefixSolid, icon, color, style),
                             solidFontFamily(), std::move(color), style, 0.75, *icon);
}

QIcon icon(RegularIcon icon, Style style)
{
    return fontawesome::icon(icon, QColor{}, style);
}

QIcon icon(RegularIcon icon, QColor color, Style style)
{
    return symbolfonts::icon(key(s_keyPrefixRegular, icon, color, style),
                             regularFontFamily(), std::move(color), style, 0.75, *icon);
}

} // namespace lmrs::gui::fontawesome
