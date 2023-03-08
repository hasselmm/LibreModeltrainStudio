#include "symbolictrackplanview.h"

#include <lmrs/core/accessories.h>
#include <lmrs/core/algorithms.h>
#include <lmrs/core/detectors.h>
#include <lmrs/core/device.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/symbolictrackplanmodel.h>
#include <lmrs/core/userliterals.h>

#include <QDomDocument>
#include <QElapsedTimer>
#include <QFile>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPicture>
#include <QRegularExpression>
#include <QSvgRenderer>

namespace lmrs::widgets {

namespace {

using core::TrackSymbol;
using core::TrackSymbolInstance;
using core::SymbolicTrackPlanModel;
using core::AccessoryControl;
using core::DetectorControl;

using namespace core::accessory;
namespace dcc = core::dcc;

constexpr auto s_bboxSize = QSizeF{10, 10};
constexpr auto s_bboxMargins = QMarginsF{0.1, 0.1, 0.1, 0.1};
constexpr auto s_bboxOuterSize = s_bboxSize.grownBy(s_bboxMargins);

static_assert(s_bboxOuterSize == QSizeF{10.2, 10.2});

template<typename T>
constexpr auto nextState(T state, core::Flags<T> validStates)
{
    constexpr auto bitCount = std::numeric_limits<std::underlying_type_t<T>>::digits;

    for (auto i = 0; i < bitCount; ++i) {
        const auto value = T{(core::value(state) + i + 1) % bitCount};
        if (core::flags(value) & validStates)
            return value;
    }

    return state;
}

static_assert(~flags(TrackSymbol::State::Undefined));
static_assert(states(TrackSymbol::Type::LeftHandPoint));
static_assert(states(TrackSymbol::Type::LeftHandPoint) ^ TrackSymbol::State::Undefined);
static_assert(states(TrackSymbol::Type::LeftHandPoint) & ~TrackSymbol::State::Undefined);
static_assert(states(TrackSymbol::Type::LeftHandPoint) == (TrackSymbol::State::Undefined | TrackSymbol::State::Straight | TrackSymbol::State::Branched));
static_assert((states(TrackSymbol::Type::LeftHandPoint) & ~flags(TrackSymbol::State::Undefined)) == (TrackSymbol::State::Straight | TrackSymbol::State::Branched));
static_assert(nextState(TrackSymbol::State::Branched, states(TrackSymbol::Type::LeftHandPoint) & ~flags(TrackSymbol::State::Undefined)) == TrackSymbol::State::Straight);

[[nodiscard]] constexpr auto suffix(TrackSymbol::State state)
{
    switch (state) {
    case TrackSymbol::State::Undefined:
        return "-undefined"_L1;
    case TrackSymbol::State::Primary:
        return "-primary"_L1;
    case TrackSymbol::State::Secondary:
        return "-secondary"_L1;
    case TrackSymbol::State::Straight:
        return "-straight"_L1;
    case TrackSymbol::State::Branched:
        return "-branched"_L1;
    case TrackSymbol::State::Crossing:
        return "-crossing"_L1;
    case TrackSymbol::State::Left:
        return "-left"_L1;
    case TrackSymbol::State::Right:
        return "-right"_L1;
    case TrackSymbol::State::Active:
        return "-active"_L1;
    case TrackSymbol::State::Red:
        return "-red"_L1;
    case TrackSymbol::State::Green:
        return "-green"_L1;
    case TrackSymbol::State::Yellow:
        return "-yellow"_L1;
    case TrackSymbol::State::White:
        return "-white"_L1;
    case TrackSymbol::State::Proceed:
        return "-proceed"_L1;
    case TrackSymbol::State::Slow:
        return "-slow"_L1;
    case TrackSymbol::State::Stop:
        return "-stop"_L1;
    }

    return QLatin1String{};
}

static_assert(!suffix(TrackSymbol::State::Undefined).isEmpty());

[[nodiscard]] constexpr auto baseId(TrackSymbol::Type symbol)
{
    switch (symbol)
    {
    case TrackSymbol::Type::Empty:
        return "empty"_L1;
    case TrackSymbol::Type::Straight:
        return "straight"_L1;
    case TrackSymbol::Type::Terminal:
        return "terminal"_L1;
    case TrackSymbol::Type::NarrowCurve:
        return "narrow-curve"_L1;
    case TrackSymbol::Type::WideCurve:
        return "wide-curve"_L1;
    case TrackSymbol::Type::Cross:
        return "cross"_L1;
    case TrackSymbol::Type::LevelCrossing:
        return "level-crossing"_L1;
    case TrackSymbol::Type::BridgeEntry:
        return "bridge-entry"_L1;
    case TrackSymbol::Type::BridgeStraight:
        return "bridge-straight"_L1;
    case TrackSymbol::Type::TunnelEntry:
        return "tunnel-entry"_L1;
    case TrackSymbol::Type::TunnelStraight:
        return "tunnel-straight"_L1;
    case TrackSymbol::Type::TunnelCurve:
        return "tunnel-curve"_L1;
    case TrackSymbol::Type::LeftHandPoint:
        return "lefthand-point"_L1;
    case TrackSymbol::Type::RightHandPoint:
        return "righthand-point"_L1;
    case TrackSymbol::Type::YPoint:
        return "y-point"_L1;
    case TrackSymbol::Type::ThreeWayPoint:
        return "threeway-point"_L1;
    case TrackSymbol::Type::SingleCrossPoint:
        return "single-cross-point"_L1;
    case TrackSymbol::Type::DoubleCrossPoint:
        return "double-cross-point"_L1;
    case TrackSymbol::Type::DoubleCrossPointSimple:
        return "double-cross-point-simple"_L1;
    case TrackSymbol::Type::TwoLightsSignal:
        return "twolights-signal"_L1;
    case TrackSymbol::Type::ThreeLightsSignal:
        return "threelights-signal"_L1;
    case TrackSymbol::Type::FourLightsSignal:
        return "fourlights-signal"_L1;
    case TrackSymbol::Type::ShuntingSignal:
        return "shunting-signal"_L1;
    case TrackSymbol::Type::SemaphoreSignal:
        return "semaphore-signal"_L1;
    case TrackSymbol::Type::Button:
        return "button"_L1;
    case TrackSymbol::Type::Decoupler:
        return "decoupler"_L1;
    case TrackSymbol::Type::Light:
        return "light"_L1;
    case TrackSymbol::Type::Switch:
        return "switch"_L1;
    case TrackSymbol::Type::Detector:
        return "response-module"_L1;
    case TrackSymbol::Type::Platform:
        return "platform"_L1;
    case TrackSymbol::Type::RedRoof:
        return "red-roof"_L1;
    case TrackSymbol::Type::BlackRoof:
        return "black-roof"_L1;
    case TrackSymbol::Type::BrownRoof:
        return "brown-roof"_L1;
    case TrackSymbol::Type::Trees:
        return "trees"_L1;
    case TrackSymbol::Type::Text:
        break; // this one has no symbol in the SVG
    }

    return QLatin1String{};
}

[[nodiscard]] constexpr auto linkId(TrackSymbol::Links links)
{
    switch (TrackSymbol::Link{links.toInt()}) {
    case TrackSymbol::Link::Center:
        return "-center"_L1;
    case TrackSymbol::Link::Cross:
        return "-cross"_L1;

    case TrackSymbol::Link::EndNorth:
        return "-end-north"_L1;
    case TrackSymbol::Link::EndEast:
        return "-end-east"_L1;
    case TrackSymbol::Link::EndSouth:
        return "-end-south"_L1;
    case TrackSymbol::Link::EndWest:
        return "-end-west"_L1;

    case TrackSymbol::Link::Horizontal:
    case TrackSymbol::Link::Vertical:
        return "-middle"_L1;

    case TrackSymbol::Link::TeeNorth:
        return "-tee-north"_L1;
    case TrackSymbol::Link::TeeEast:
        return "-tee-east"_L1;
    case TrackSymbol::Link::TeeSouth:
        return "-tee-south"_L1;
    case TrackSymbol::Link::TeeWest:
        return "-tee-west"_L1;

    case TrackSymbol::Link::CornerNorthWest:
        return "-corner-northwest"_L1;
    case TrackSymbol::Link::CornerNorthEast:
        return "-corner-northeast"_L1;
    case TrackSymbol::Link::CornerSouthEast:
        return "-corner-southeast"_L1;
    case TrackSymbol::Link::CornerSouthWest:
        return "-corner-southwest"_L1;

    case TrackSymbol::Link::None:
    case TrackSymbol::Link::ConnectNorth:
    case TrackSymbol::Link::ConnectEast:
    case TrackSymbol::Link::ConnectSouth:
    case TrackSymbol::Link::ConnectWest:
        return QLatin1String{};
    }

    return "-<invalid id requested>"_L1;
}

[[nodiscard]] auto boundsId(TrackSymbol::Type symbol)
{
    return "bounds-"_L1 + baseId(symbol);
}

[[nodiscard]] auto symbolId(TrackSymbol::Type symbol)
{
    return "symbol-"_L1 + baseId(symbol);
}

[[nodiscard]] auto boundsId(TrackSymbol::Type symbol, TrackSymbol::Links links)
{
    return boundsId(symbol) + linkId(links);
}

[[nodiscard]] auto symbolId(TrackSymbol::Type symbol, TrackSymbol::Links links)
{
    return symbolId(symbol) + linkId(links);
}

[[nodiscard]] auto highlightId(TrackSymbol::Type symbol, TrackSymbol::State state)
{
    return "highlight-"_L1 + baseId(symbol) + suffix(state);
}

[[nodiscard]] auto labelId(TrackSymbol::Type symbol, TrackSymbol::State state)
{
    return "label-"_L1 + baseId(symbol) + suffix(state);
}

static_assert(!baseId(TrackSymbol::Type::Empty).isEmpty());
static_assert(baseId(TrackSymbol::Type::Text).isEmpty());

constexpr auto pathColor(TrackSymbol::Mode mode)
{
    switch (mode) {
    case TrackSymbol::Mode::Normal:
        break;

    case TrackSymbol::Mode::Active:
        break;

    case TrackSymbol::Mode::Disabled:
        return QColorConstants::Svg::dimgray;
    }

    return QColor{};
}

constexpr auto highlightColor(TrackSymbol::Mode mode)
{
    switch (mode) {
    case TrackSymbol::Mode::Normal:
        return QColorConstants::Svg::slategray;

    case TrackSymbol::Mode::Active:
        return QColorConstants::Svg::steelblue;

    case TrackSymbol::Mode::Disabled:
        return QColorConstants::Svg::darkgray;
    }

    return QColor{};
}

auto isHiddenHighlight(QDomElement element, TrackSymbol::Mode mode, TrackSymbol::State state)
{
    if (const auto id = element.attribute("id"_L1); id.startsWith("highlight-"_L1)) {
        if (!id.endsWith(suffix(state)))
            return true;
        else if (id.endsWith(suffix(TrackSymbol::State::Undefined))
                && mode != TrackSymbol::Mode::Active)
            return true;
    }

    return false;
}

void hide(QDomElement element)
{
    auto style = element.attribute("style"_L1).split(';'_L1);

    style.removeOne("display:inline"_L1);

    if (!style.contains("display:none"_L1))
        style.append("display:none"_L1);

    element.setAttribute("style"_L1, style.join(';'_L1));

    for (auto child = element.firstChildElement(); child.isElement(); child = child.nextSiblingElement())
        hide(child);
}

void adjustStyle(QDomElement element, TrackSymbol::Mode mode, TrackSymbol::State state)
{
    if (isHiddenHighlight(element, mode, state)) {
        hide(element);
        return;
    }

    auto style = element.attribute("style"_L1).split(';'_L1);
    auto modified = false;

    if (const auto i = style.indexOf("stroke:#80c000"_L1); i >= 0) {
        if (const auto color = highlightColor(mode); color.isValid()) {
            style[i] = "stroke:"_L1 + color.name();
            modified = true;
        }
    } else if (const auto i = style.indexOf("stroke:#000000"_L1); i >= 0) {
        if (const auto color = pathColor(mode); color.isValid()) {
            style[i] = "stroke:"_L1 + color.name();
            modified = true;
        }
    } else if (const auto i = style.indexOf("fill:#000000"_L1); i >= 0) {
        if (const auto color = pathColor(mode); color.isValid()) {
            style[i] = "fill:"_L1 + color.name();
            modified = true;
        }
    }

    if (modified) {
        element.setAttribute("style"_L1, style.join(';'_L1));
        return;
    }

    for (auto child = element.firstChildElement(); child.isElement(); child = child.nextSiblingElement())
        adjustStyle(child, mode, state);
}

QByteArray adjustStyle(QByteArray content, TrackSymbol::Mode mode, TrackSymbol::State state)
{
    auto document = QDomDocument{};
    document.setContent(content, false);
    adjustStyle(document.documentElement(), mode, state);
    return document.toByteArray();
}

auto paintGuard(QPainter *painter)
{
    painter->save();

    return qScopeGuard([painter] {
        painter->restore();
    });
}

struct CellPosition
{
    int row;
    int column;

    [[nodiscard]] constexpr bool operator==(const CellPosition &) const noexcept = default;
};

QDebug operator<<(QDebug debug, const CellPosition &pos)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(pos)>{debug};
    return debug << "row=" << pos.row << ", column=" << pos.column;
}

} // namespace

class SymbolicTrackPlanView::Private : public core::PrivateObject<SymbolicTrackPlanView>
{
public:
    using PrivateObject::PrivateObject;

    SymbolicTrackPlanView *q() const { return core::checked_cast<SymbolicTrackPlanView *>(parent()); }

    [[nodiscard]] auto itemAt(QPoint p) const { return CellPosition{qFloor(p.y() / tileSize), qFloor(p.x() / tileSize)}; }
    [[nodiscard]] std::function<void()> cellAction(CellPosition pos) const;
    [[nodiscard]] TrackSymbolInstance cellAt(CellPosition pos) const;

    [[nodiscard]] auto tile(int r, int c) const { return QRectF{c * tileSize, r * tileSize, tileSize, tileSize}; }
    [[nodiscard]] auto gridScale() const { return static_cast<qreal>(tileSize) * 0.1f; }
    [[nodiscard]] auto clipPath(QRectF box);


    using TileVisitor = std::function<void(QRectF, TrackSymbolInstance)>;
    void visitTiles(QRegion region, TileVisitor visit);

    void drawSymbol(QPainter *painter, TrackSymbolInstance instance, QRectF bounds);
    void drawLabels(QPainter *painter, TrackSymbolInstance instance, QRectF bounds);

    [[nodiscard]] QByteArray readSymbols() const;
    void validateSymbolIds(TrackSymbol::Type symbol, QSvgRenderer *renderer);
    void collectPositions(TrackSymbol::State state, QSvgRenderer *renderer);
    void updatePictureCache(TrackSymbol symbol, QSvgRenderer *renderer);

    void setFontSize(QPainter *painter, qreal points) const;

    enum LayoutDetail {
        ColumnsInserted,
        ColumnsRemoved,
        RowsInserted,
        RowsRemoved,
    };

    void onDataChanged(QModelIndex topLeft, QModelIndex bottomRight, QList<int> roles);

    void onAccessoryInfoChanged(AccessoryInfo info);
    void onDetectorInfoChanged(DetectorInfo info);
    void onTurnoutInfoChanged(TurnoutInfo info);

    QHash<TrackSymbol::Type, QList<QPointF>> labelPositions;    // FIXME: share among instances
    QHash<TrackSymbol, QPicture> pictures;                      // FIXME: share among instances

    qreal tileSize = 50;
    QColor gridColor = Qt::GlobalColor::lightGray;

    QPointer<SymbolicTrackPlanModel> model;
    QPointer<AccessoryControl> accessoryControl;
    QPointer<DetectorControl> detectorControl;

    CellPosition pressed;
};

std::function<void ()> SymbolicTrackPlanView::Private::cellAction(CellPosition pos) const
{
    const auto cell = cellAt(pos);
    const auto states = core::states(cell.symbol.type) & ~TrackSymbol::State::Undefined;
    const auto newState = nextState(cell.symbol.state, states);

    const auto fallbackAction = [this, pos, newState] {
        auto value = QVariant::fromValue(newState);
        auto index = model->index(pos.row, pos.column);
        model->setData(std::move(index), std::move(value), SymbolicTrackPlanModel::StateRole);
    };

    if (const auto turnoutState = core::turnoutState(newState);
            turnoutState != dcc::TurnoutState::Unknown) {
        if (const auto address = cell.turnoutAddress()) {
            return [this, address, turnoutState, cellType = cell.symbol.type, fallbackAction] {
                qCInfo(logger(), "switching %s with address %d to %s state",
                       core::key(cellType), value(address.value()),
                       core::key(turnoutState));

                if (accessoryControl)
                    accessoryControl->setTurnoutState(address.value(), turnoutState);
                else
                    fallbackAction();
            };
        }
    }

    return {};
}

TrackSymbolInstance SymbolicTrackPlanView::Private::cellAt(CellPosition pos) const
{
    if (const auto index = model ? model->index(pos.row, pos.column) : QModelIndex{}; index.isValid())
        return qvariant_cast<TrackSymbolInstance>(index.data(SymbolicTrackPlanModel::CellRole));

    return {};
}

auto SymbolicTrackPlanView::Private::clipPath(QRectF box)
{
    const auto dx = QPointF{gridScale(), 0};
    const auto dy = QPointF{0, gridScale()};
    auto clipPath = QPainterPath{};

    clipPath.moveTo(box.topLeft()     + dx - dy);
    clipPath.lineTo(box.topLeft()     + dx + dx);
    clipPath.lineTo(box.topRight()    - dx - dx);
    clipPath.lineTo(box.topRight()    - dx - dy);
    clipPath.lineTo(box.topRight()    + dx + dy);
    clipPath.lineTo(box.topRight()    + dy + dy);
    clipPath.lineTo(box.bottomRight() - dy - dy);
    clipPath.lineTo(box.bottomRight() + dx - dy);
    clipPath.lineTo(box.bottomRight() - dx + dy);
    clipPath.lineTo(box.bottomRight() - dx - dx);
    clipPath.lineTo(box.bottomLeft()  + dx + dx);
    clipPath.lineTo(box.bottomLeft()  + dx + dy);
    clipPath.lineTo(box.bottomLeft()  - dx - dy);
    clipPath.lineTo(box.bottomLeft()  - dy - dy);
    clipPath.lineTo(box.topLeft()     + dy + dy);
    clipPath.lineTo(box.topLeft()     - dx + dy);
    clipPath.closeSubpath();

    return clipPath;
}

void SymbolicTrackPlanView::Private::visitTiles(QRegion region, TileVisitor visit)
{
    if (Q_UNLIKELY(!model))
        return;

    for (auto row = 0, rowCount = model->rowCount(); row < rowCount; ++row) {
        for (auto column = 0, columnCount = model->columnCount(); column < columnCount; ++column) {
            if (auto rect = tile(row, column); region.contains(rect.toRect())) {
                auto cell = model->index(row, column).data(SymbolicTrackPlanModel::CellRole);
                visit(std::move(rect), qvariant_cast<TrackSymbolInstance>(std::move(cell)));
            }
        }
    }
}

void SymbolicTrackPlanView::Private::drawSymbol(QPainter *painter, TrackSymbolInstance instance, QRectF bounds)
{
    const auto guard = paintGuard(painter);

    // apply clip path
    if (instance.symbol.type != TrackSymbol::Type::Text)
        painter->setClipPath(clipPath(bounds));

    if (instance.symbol.type != TrackSymbol::Type::Text) {
        // apply angle
        painter->translate(bounds.center());
        painter->rotate(value(instance.orientation));
        painter->translate(-bounds.center());

        // align symbol
        painter->translate(bounds.topLeft());
        painter->scale(gridScale(), gridScale());

        // paint symbol
        if (auto picture = pictures[std::move(instance.symbol)]; !picture.isNull()) {
            painter->drawPicture(0, 0, std::move(picture));
        } else {
            qCWarning(logger(),
                      "No picture available for (type=%s, mode=%s, state=%s, links=%s)",
                      core::key(instance.symbol.type), core::key(instance.symbol.mode),
                      core::key(instance.symbol.state), core::keys(instance.symbol.links).constData());
        }

    } else if (auto list = instance.detail.toList(); list.size() >= 2
               && list[0].typeId() == qMetaTypeId<QString>()
               && list[1].typeId() == qMetaTypeId<int>()) {
        setFontSize(painter, list[1].toInt());
        painter->drawText(bounds, Qt::AlignCenter | Qt::TextDontClip, list[0].toString());
    } else if (auto text = instance.detail.toString(); !text.isEmpty()) {
        painter->drawText(bounds, Qt::AlignCenter | Qt::TextDontClip, std::move(text));
    }
}

void SymbolicTrackPlanView::Private::drawLabels(QPainter *painter, TrackSymbolInstance instance, QRectF bounds)
{
    auto labels = QStringList{};

    if (instance.detail.canConvert<QString>()) {
        labels += instance.detail.toString();
    } else if (instance.detail.canConvert<QList<int>>()) {
        for (const auto index: qvariant_cast<QList<int>>(instance.detail))
            labels += QString::number(index);
    }

    const auto positions = labelPositions[instance.symbol.type];
    const auto center = QPointF{bounds.width(), bounds.height()} * 0.5;
    const auto transform = QTransform{}.rotate(value(instance.orientation));

    for (auto i = qsizetype{0}, l = qMin(labels.size(), positions.size()); i < l; ++i) {
        // retreive label bounds and center them
        auto labelBounds = painter->fontMetrics().boundingRect(labels[i]).toRectF();
        labelBounds.moveTo(-labelBounds.width() * 0.5, -labelBounds.height() * 0.5);

        // apply rotation and move to rectangle described by bounds
        labelBounds.translate((positions[i] * gridScale() - center) * transform + bounds.center());

        // add space for rounded borders to label boundaries
        const auto r = labelBounds.height() * 0.5;
        labelBounds += QMarginsF{r * 0.5, 1, r * 0.5, 1};

        // avoid vertically oriented ellipses for small numbers
        labelBounds.setWidth(qMax(labelBounds.width(), labelBounds.height()));

        // prepare path for rendering rounded rectangle
        auto backgroundPath = QPainterPath{};
        backgroundPath.addRoundedRect(labelBounds, r, r);

        // draw all the things we prepared
        painter->fillPath(backgroundPath, Qt::GlobalColor::white);
        painter->drawPath(backgroundPath);
        painter->drawText(labelBounds, Qt::AlignCenter | Qt::TextDontClip, labels[i]);
    }
}

QByteArray SymbolicTrackPlanView::Private::readSymbols() const
{
    auto file = QFile{":/taschenorakel.de/lmrs/widgets/assets/track-symbols.svg"_L1};

    if (!file.open(QFile::ReadOnly)) {
        qCWarning(logger(), "Could not load symbols: %ls", qUtf16Printable(file.errorString()));
        return {};
    }

    return file.readAll();
}

void SymbolicTrackPlanView::Private::validateSymbolIds(TrackSymbol::Type symbol, QSvgRenderer *renderer)
{
    auto expectedIds = QStringList{};

    const auto boundsId = widgets::boundsId(symbol);
    const auto symbolId = widgets::symbolId(symbol);

    if (hasLinks(symbol)) {
        for (const auto &link: QMetaTypeId<TrackSymbol::Link>()) {
            if (TrackSymbol::Links{link}.testFlag(TrackSymbol::Link::Center)) {
                expectedIds += widgets::boundsId(symbol, link.value());
                expectedIds += widgets::symbolId(symbol, link.value());
            }
        }
    } else {
        expectedIds += {boundsId, symbolId};

        for (const auto &state: QMetaTypeId<TrackSymbol::State>()) {
            if (states(symbol) & state.value())
                expectedIds += highlightId(symbol, state);
            if (labels(symbol) & state.value())
                expectedIds += labelId(symbol, state);
        }
    }

    for (const auto &id: expectedIds) {
        if (!renderer->elementExists(id)) {
            qCWarning(logger(), "Could not find SVG element \"%ls\" for symbol %s",
                      qUtf16Printable(id), core::key(symbol));
        }
    }

    if (const auto size = renderer->boundsOnElement(boundsId).size();
            renderer->elementExists(boundsId) && size != s_bboxOuterSize) {
        qCWarning(logger(), "Symbol %s has invalid size: (%gx%g)",
                  core::key(symbol), size.width(), size.height());
    }
}

void SymbolicTrackPlanView::Private::updatePictureCache(TrackSymbol symbol, QSvgRenderer *renderer)
{
    const auto boundsId = widgets::boundsId(symbol.type, symbol.links);
    const auto boundsRect = renderer->boundsOnElement(boundsId) - s_bboxMargins;

    const auto symbolId = widgets::symbolId(symbol.type, symbol.links);
    const auto symbolRect = renderer->boundsOnElement(symbolId);

    auto picture = QPicture{};

    {
        auto painter = QPainter{&picture};
        auto origin = symbolRect.topLeft() - boundsRect.topLeft();
        renderer->render(&painter, symbolId, {std::move(origin), symbolRect.size()});
    }

    if (picture.boundingRect().isEmpty() && symbol.type != TrackSymbol::Type::Empty) {
        qCWarning(logger(), "Discarding empty picture for symbol %ls/%ls",
                  qUtf16Printable(symbolId), qUtf16Printable(boundsId));
    } else {
        pictures[std::move(symbol)] = std::move(picture);
    }
}

void SymbolicTrackPlanView::Private::collectPositions(TrackSymbol::State state, QSvgRenderer *renderer)
{
    for (const auto &type: QMetaTypeId<TrackSymbol::Type>()) {
        if (labels(type) & state) {
            const auto boundsId = widgets::boundsId(type);
            const auto boundsRect = renderer->boundsOnElement(boundsId) - s_bboxMargins;
            const auto labelId = widgets::labelId(type, state);

            auto position
                    = renderer->boundsOnElement(labelId).center()
                    * renderer->transformForElement(labelId)
                    - boundsRect.topLeft();

            labelPositions[type] += std::move(position);
        }
    }
}

void SymbolicTrackPlanView::Private::setFontSize(QPainter *painter, qreal points) const
{
    auto font = painter->font();
    font.setPointSizeF(qMin(1.0, (tileSize + 50) / 150.0) * points);
    painter->setFont(std::move(font));
}

void SymbolicTrackPlanView::Private::onDataChanged(QModelIndex topLeft, QModelIndex bottomRight, QList<int>)
{
    const auto topLeftPoint = tile(topLeft.row(), topLeft.column()).topLeft();
    const auto bottomRightPoint = tile(bottomRight.row(), bottomRight.column()).bottomRight();
    const auto updateArea = QRectF{topLeftPoint, bottomRightPoint}.toAlignedRect();
    const auto updateMargin = qCeil(tileSize/2);

    q()->update(updateArea.marginsAdded({updateMargin, updateMargin, updateMargin, updateMargin}));
}

void SymbolicTrackPlanView::Private::onAccessoryInfoChanged(AccessoryInfo info)
{
    qCInfo(logger()) << info;
}

void SymbolicTrackPlanView::Private::onDetectorInfoChanged(DetectorInfo info)
{
    qCInfo(logger()) << info;

    if (!model)
        return;

    using Occupancy = DetectorInfo::Occupancy;
    using SymbolState = TrackSymbol::State;

    const auto newState = info.occupancy() == Occupancy::Occupied ? SymbolState::Active : SymbolState::Undefined;
    const auto newStateVariant = QVariant::fromValue(newState);

    for (const auto &index: model->findDetectors(info.address()))
        model->setData(index, newStateVariant, SymbolicTrackPlanModel::DataRole::StateRole);
}

void SymbolicTrackPlanView::Private::onTurnoutInfoChanged(TurnoutInfo info)
{
    qCInfo(logger()) << info;

    if (!model)
        return;

    using SymbolState = TrackSymbol::State;

    for (const auto &index: model->findAccessories(info.address())) {
        const auto cell = qvariant_cast<TrackSymbolInstance>(index.data(SymbolicTrackPlanModel::CellRole));
        auto newState = cell.symbol.state;

        switch (info.state()) {
        case dcc::TurnoutState::Straight:
            for (auto state: {SymbolState::Straight, SymbolState::Left, SymbolState::Red, SymbolState::Stop}) {
                if (states(cell.symbol.type) & state) {
                    newState = state;
                    break;
                }
            }

            break;

        case dcc::TurnoutState::Branched:
            for (auto state: {SymbolState::Branched, SymbolState::Right, SymbolState::Green, SymbolState::Proceed}) {
                if (states(cell.symbol.type) & state) {
                    newState = state;
                    break;
                }
            }

            break;

        case dcc::TurnoutState::Unknown:
        case dcc::TurnoutState::Invalid:
            break;
        }

        qCInfo(logger()) << cell.symbol.type << cell.symbol.state << "=>" << newState;

        if (newState != cell.symbol.state)
            model->setData(index, QVariant::fromValue(newState), SymbolicTrackPlanModel::StateRole);
    }
}

SymbolicTrackPlanView::SymbolicTrackPlanView(QWidget *parent)
    : QWidget{parent}
    , d{new Private{this}}
{
    setMouseTracking(true);

    auto t = QElapsedTimer{};
    t.start();

    const auto content = d->readSymbols();

    if (Q_UNLIKELY(content.isEmpty()))
        return;

    for (const auto &mode: QMetaTypeId<TrackSymbol::Mode>()) {
        for (const auto &state: QMetaTypeId<TrackSymbol::State>()) {
            auto renderer = QSvgRenderer{adjustStyle(content, mode, state)};

            if (!renderer.isValid()) {
                qCWarning(d->logger(), "Could not load track plan symbols");
                return;
            }

            for (const auto &type: QMetaTypeId<TrackSymbol::Type>()) {
                if ((states(type) & state.value()) == 0)
                    continue;
                if (type == TrackSymbol::Type::Text)
                    continue;

                if (mode == TrackSymbol::Mode::Normal)
                    d->validateSymbolIds(type, &renderer);

                if (hasLinks(type)) {
                    for (const auto &link: QMetaTypeId<TrackSymbol::Link>()) {
                        if (TrackSymbol::Links{link}.testFlag(TrackSymbol::Link::Center))
                            d->updatePictureCache({type, mode, state, link.value()}, &renderer);
                    }
                } else {
                    d->updatePictureCache({type, mode, state}, &renderer);
                }
            }

            if (mode == TrackSymbol::Mode::Normal)
                d->collectPositions(state, &renderer);
        }
    }

    qCInfo(d->logger()) << "time for loading symbols:" << std::chrono::milliseconds{t.elapsed()};
    setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
}

void SymbolicTrackPlanView::setModel(SymbolicTrackPlanModel *newModel)
{
    const auto guard = core::propertyGuard(this, &SymbolicTrackPlanView::model,
                                           &SymbolicTrackPlanView::modelChanged);

    if (const auto oldModel = std::exchange(d->model, newModel); oldModel != newModel) {
        if (oldModel)
            oldModel->disconnect(this);

        if (newModel) {
            connect(newModel, &SymbolicTrackPlanModel::columnsInserted, this, &SymbolicTrackPlanView::adjustSize);
            connect(newModel, &SymbolicTrackPlanModel::columnsRemoved, this, &SymbolicTrackPlanView::adjustSize);
            connect(newModel, &SymbolicTrackPlanModel::rowsInserted, this, &SymbolicTrackPlanView::adjustSize);
            connect(newModel, &SymbolicTrackPlanModel::rowsRemoved, this, &SymbolicTrackPlanView::adjustSize);
            connect(newModel, &SymbolicTrackPlanModel::modelReset, this, &SymbolicTrackPlanView::adjustSize);
            connect(newModel, &SymbolicTrackPlanModel::dataChanged, d, &Private::onDataChanged);
        }

        adjustSize();
    }
}

SymbolicTrackPlanModel *SymbolicTrackPlanView::model() const
{
    return d->model;
}

void SymbolicTrackPlanView::setAccessoryControl(AccessoryControl *newControl)
{
    const auto guard = core::propertyGuard(this, &SymbolicTrackPlanView::accessoryControl,
                                           &SymbolicTrackPlanView::accessoryControlChanged);

    if (const auto oldControl = std::exchange(d->accessoryControl, newControl); oldControl != newControl) {
        if (oldControl)
            oldControl->disconnect(this);

        if (newControl) {
            connect(newControl, &AccessoryControl::accessoryInfoChanged, d, &Private::onAccessoryInfoChanged);
            connect(newControl, &AccessoryControl::turnoutInfoChanged, d, &Private::onTurnoutInfoChanged);
            // FIXME: query initial accessory and turnout state?
        }

        adjustSize();
    }
}

AccessoryControl *SymbolicTrackPlanView::accessoryControl() const
{
    return d->accessoryControl;
}

void SymbolicTrackPlanView::setDetectorControl(DetectorControl *newControl)
{
    const auto guard = core::propertyGuard(this, &SymbolicTrackPlanView::detectorControl,
                                           &SymbolicTrackPlanView::detectorControlChanged);

    if (const auto oldControl = std::exchange(d->detectorControl, newControl); oldControl != newControl) {
        if (oldControl)
            oldControl->disconnect(this);

        if (newControl) {
            connect(newControl, &DetectorControl::detectorInfoChanged, d, &Private::onDetectorInfoChanged);
            // FIXME: query initial detector state?
        }

        adjustSize();
    }
}

DetectorControl *SymbolicTrackPlanView::detectorControl() const
{
    return d->detectorControl;
}

void SymbolicTrackPlanView::setTileSize(int newSize)
{
    const auto guard = core::propertyGuard(this, &SymbolicTrackPlanView::tileSize,
                                           &SymbolicTrackPlanView::tileSizeChanged);

    if (const auto oldSize = std::exchange(d->tileSize, newSize); oldSize != newSize)
        adjustSize();
}

int SymbolicTrackPlanView::tileSize() const
{
    return qRound(d->tileSize);
}

QSize SymbolicTrackPlanView::minimumSizeHint() const
{
    if (d->model) {
        const auto width = qMax(1, d->model->columnCount()) * tileSize() + 1;
        const auto height = qMax(1, d->model->rowCount()) * tileSize() + 1;
        return {width, height};
    }

    return {};
}

QSize SymbolicTrackPlanView::sizeHint() const
{
    return minimumSizeHint();
}

void SymbolicTrackPlanView::mousePressEvent(QMouseEvent *event)
{
    d->pressed = d->itemAt(event->pos());
    QWidget::mousePressEvent(event);
}

void SymbolicTrackPlanView::mouseReleaseEvent(QMouseEvent *event)
{
    if (auto clicked = d->itemAt(event->pos()); clicked == d->pressed) {
        if (const auto cellAction = d->cellAction(std::move(clicked)))
            cellAction();

        event->accept();
    }

    QWidget::mouseReleaseEvent(event);
}

void SymbolicTrackPlanView::mouseMoveEvent(QMouseEvent *event)
{
    const auto cellAction = d->cellAction(d->itemAt(event->pos()));
    setCursor(cellAction ? Qt::PointingHandCursor : QCursor{});
    QWidget::mouseMoveEvent(event);
}

void SymbolicTrackPlanView::paintEvent(QPaintEvent *event)
{
    auto painter = QPainter{this};
    painter.setRenderHint(QPainter::Antialiasing);
    d->setFontSize(&painter, 9);

    {
        const auto guard = paintGuard(&painter);
        painter.setPen(d->gridColor);

        const auto w = static_cast<float>(width());
        const auto h = static_cast<float>(height());
        const auto s = static_cast<float>(tileSize());

        for (auto line = QLineF{0, 0, w, 0}; line.y1() < h; line.translate(0, s))
            painter.drawLine(line);
        for (auto line = QLineF{0, 0, 0, h}; line.x1() < w; line.translate(s, 0))
            painter.drawLine(line);
    }

    d->visitTiles(event->region(), [this, &painter](QRectF rect, TrackSymbolInstance cell) {
        if (cell.symbol.type != TrackSymbol::Type::Empty
                && cell.symbol.type != TrackSymbol::Type::Text) {
            painter.fillPath(d->clipPath(std::move(rect)), QColorConstants::Svg::pink);
        }
    });

    d->visitTiles(event->region(), [this, &painter](QRectF rect, TrackSymbolInstance cell) {
        d->drawSymbol(&painter, std::move(cell), rect);
    });

    d->visitTiles(event->region(), [this, &painter](QRectF rect, TrackSymbolInstance cell) {
        d->drawLabels(&painter, std::move(cell), rect);
    });
}

} // namespace lmrs::widgets
