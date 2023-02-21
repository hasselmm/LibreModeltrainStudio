#include "symbolfonts.h"

#include <QGuiApplication>
#include <QIconEngine>
#include <QPainter>
#include <QPalette>

namespace lmrs::gui::symbolfonts {

namespace {

auto colorGroup(QIcon::Mode mode)
{
    switch (mode) {
    case QIcon::Mode::Normal:
        break;

    case QIcon::Mode::Active:
        return QPalette::ColorGroup::Active;
    case QIcon::Mode::Selected:
        return QPalette::ColorGroup::Current;
    case QIcon::Mode::Disabled:
        return QPalette::ColorGroup::Disabled;
    }

    return QPalette::ColorGroup::Normal;
}

QColor defaultColor(QIcon::Mode mode)
{
    return qApp->palette().color(colorGroup(mode), QPalette::Window);
}

QColor defaultTextColor(QIcon::Mode mode)
{
    return qApp->palette().color(colorGroup(mode), QPalette::ButtonText);
}

QColor defaultSelectedColor(QIcon::Mode mode)
{
    return qApp->palette().color(colorGroup(mode), QPalette::Highlight);
}

QColor defaultSelectedTextColor(QIcon::Mode mode)
{
    return qApp->palette().color(colorGroup(mode), QPalette::HighlightedText);
}

class SymbolFontIconEngine : public QIconEngine
{
public:
    explicit SymbolFontIconEngine(QString key, QString fontFamily, QColor color, Style style, qreal scale, QString text)
        : m_fontFamily{std::move(fontFamily)}
        , m_text{std::move(text)}
        , m_key{std::move(key)}
        , m_color{std::move(color)}
        , m_style{style}
        , m_scale{scale}
    {}

public: // QIconEngine interface
    QString key() const override { return m_key; }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override;
    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state, qreal scale);
    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override;
    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override;
    bool isNull() override { return false; }
    QIconEngine *clone() const override;

    void virtual_hook(int id, void *data) override;

private:
    QColor textColor(QIcon::Mode mode, QIcon::State state) const;
    QColor fillColor(QIcon::Mode mode) const;

    QString m_fontFamily;
    QString m_text;
    QString m_key;
    QColor m_color;
    Style m_style;
    qreal m_scale;
};

void SymbolFontIconEngine::paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state)
{
    paint(painter, rect, mode, state, 1);
}

void SymbolFontIconEngine::paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state, qreal scale)
{
    auto scaledRect = QRectF{rect.x() / scale, rect.y() / scale, rect.width() / scale, rect.height() / scale};

    auto font = QFont{m_fontFamily};
    font.setPixelSize(qRound(scaledRect.height() * m_scale));

    painter->setFont(std::move(font));
    painter->setPen(textColor(mode, state));
    painter->drawText(std::move(scaledRect), m_text, Qt::AlignHCenter | Qt::AlignVCenter);
}

QPixmap SymbolFontIconEngine::scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale)
{
    auto pixmap = QPixmap{size};
    pixmap.setDevicePixelRatio(scale);
    pixmap.fill(fillColor(mode));

    auto painter = QPainter{&pixmap};
    paint(&painter, {{0, 0}, size}, mode, state, scale);
    painter.end();

    return pixmap;
}

QPixmap SymbolFontIconEngine::pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state)
{
    auto pixmap = QPixmap{size};
    pixmap.fill(fillColor(mode));

    auto painter = QPainter{&pixmap};
    paint(&painter, {{0, 0}, size}, mode, state);
    painter.end();

    return pixmap;
}

QIconEngine *SymbolFontIconEngine::clone() const
{
    return new SymbolFontIconEngine{m_key, m_fontFamily, m_color, m_style, m_scale, m_text};
}

void SymbolFontIconEngine::virtual_hook(int id, void *data)
{
    switch (static_cast<IconEngineHook>(id)) {
    case IconEngineHook::IsNullHook:
        *static_cast<bool *>(data) = isNull();
        return;

    case IconEngineHook::ScaledPixmapHook:
        if (const auto arg = static_cast<ScaledPixmapArgument *>(data))
            arg->pixmap = scaledPixmap(arg->size, arg->mode, arg->state, arg->scale);

        return;
    }

    QIconEngine::virtual_hook(id, data);
}

QColor SymbolFontIconEngine::textColor(QIcon::Mode mode, QIcon::State state) const
{
    if (m_color.isValid())
        return m_color;

    switch (m_style) {
    case Style::Inverted:
        return defaultColor(mode);
    case Style::Selected:
        return defaultSelectedTextColor(mode);
    case Style::Normal:
        break;
    }

    if (state == QIcon::State::On)
        return defaultSelectedColor(mode);

    return defaultTextColor(mode);
}

QColor SymbolFontIconEngine::fillColor(QIcon::Mode mode) const
{
    switch (m_style) {
    case Style::Inverted:
        return defaultTextColor(mode);
    case Style::Selected:
        return defaultSelectedColor(mode);
    case Style::Normal:
        break;
    }

    return Qt::transparent;
}

} // namespace

QIcon icon(QString key, QString fontName, QColor color, Style style, qreal scale, QString text)
{
    return QIcon{new SymbolFontIconEngine{std::move(key), std::move(fontName),
                    std::move(color), style, scale, std::move(text)}};
}

QIcon icon(QString key, QString fontName, Style style, qreal scale, QString text)
{
    return icon(std::move(key), std::move(fontName), QColor{}, style, scale, std::move(text));
}

} // namespace lmrs::gui::symbolfonts
