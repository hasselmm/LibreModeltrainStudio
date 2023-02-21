#ifndef LMRS_CORE_SYMBOLICTRACKPLANMODEL_H
#define LMRS_CORE_SYMBOLICTRACKPLANMODEL_H

#include "fileformat.h"

#include <lmrs/core/dccconstants.h>

#include <QAbstractTableModel>

namespace lmrs::core {

namespace rm {
struct DetectorAddress;
}

struct FileFormat;

struct TrackSymbol
{
    Q_GADGET
    Q_PROPERTY(Type type MEMBER type CONSTANT FINAL)
    Q_PROPERTY(Mode mode MEMBER mode CONSTANT FINAL)
    Q_PROPERTY(State state MEMBER state CONSTANT FINAL)

public:
    enum class Type {
        Empty,
        Straight,
        Terminal,
        NarrowCurve,
        WideCurve,
        Cross,                  // diamond crossing
        LeftHandPoint,
        RightHandPoint,
        YPoint,
        ThreeWayPoint,
        SingleCrossPoint,       // single slip switch crossing, two point machines
        DoubleCrossPoint,       // double slip switch crossing, two point machines
        DoubleCrossPointSimple, // double slip switch crossing, single point machine
        LevelCrossing,
        BridgeEntry,
        BridgeStraight,
        TunnelEntry,
        TunnelStraight,
        TunnelCurve,
        TwoLightsSignal,
        ThreeLightsSignal,
        FourLightsSignal,
        ShuntingSignal,
        SemaphoreSignal,
        Detector,
        Decoupler,
        Light,
        Button,
        Switch,
//        SignalExtended,
//        SignalZ21Link,
        Platform,
        RedRoof,
        BlackRoof,
        BrownRoof,
        Trees,
        Text,
    };

    Q_ENUM(Type)

    enum class Mode {
        Normal,
        Disabled,
        Active,
    };

    Q_ENUM(Mode)

    enum class State {
        Undefined,
        Primary,
        Secondary,
        Straight,
        Branched,
        Crossing,
        Left,
        Right,
        Active,
        Green,
        Red,
        Yellow,
        White,
        Proceed,
        Slow,
        Stop,
    };

    Q_ENUM(State)

    enum class Link
    {
        None =              0,

        Center =            1 << 0,
        ConnectNorth =      1 << 1,
        ConnectEast =       1 << 2,
        ConnectSouth =      1 << 3,
        ConnectWest =       1 << 4,

        EndNorth =          Center | ConnectSouth,
        EndEast =           Center | ConnectWest,
        EndSouth =          Center | ConnectNorth,
        EndWest =           Center | ConnectEast,

        Horizontal =        Center | ConnectEast  | ConnectWest,
        Vertical =          Center | ConnectNorth | ConnectSouth,

        TeeNorth =          Center | Horizontal   | ConnectNorth,
        TeeEast =           Center | Vertical     | ConnectEast,
        TeeSouth =          Center | Horizontal   | ConnectSouth,
        TeeWest =           Center | Vertical     | ConnectWest,

        CornerNorthWest =   Center | ConnectEast  | ConnectSouth,
        CornerNorthEast =   Center | ConnectWest  | ConnectSouth,
        CornerSouthEast =   Center | ConnectWest  | ConnectNorth,
        CornerSouthWest =   Center | ConnectEast  | ConnectNorth,

        Cross =             Center | ConnectNorth | ConnectEast | ConnectSouth | ConnectWest,
    };

    Q_FLAG(Link)
    Q_DECLARE_FLAGS(Links, Link)

    Type type;
    Mode mode = Mode::Normal;
    State state = State::Undefined;
    Links links = Link::None;

    constexpr auto fields() const noexcept { return std::tie(type, mode, state, links); }

    constexpr auto operator==(const TrackSymbol &rhs) const noexcept { return fields() == rhs.fields(); }
    constexpr auto operator!=(const TrackSymbol &rhs) const noexcept { return fields() != rhs.fields(); }

    operator QVariant() const { return QVariant::fromValue(*this); }
};

struct TrackSymbolInstance
{
    Q_GADGET
    Q_PROPERTY(TrackSymbol symbol MEMBER symbol CONSTANT FINAL)
    Q_PROPERTY(Orientation orientation MEMBER orientation CONSTANT FINAL)
    Q_PROPERTY(QVariant detail MEMBER detail CONSTANT FINAL)

public:
    enum class Orientation {
        North =       0,
        NorthEast =  45,
        East =       90,
        SouthEast = 135,
        South =     180,
        SouthWest = 225,
        West =      270,
        NorthWest = 315,
    };

    Q_ENUM(Orientation)

    TrackSymbolInstance() noexcept = default;

    TrackSymbolInstance(TrackSymbol symbol, QVariant detail = {}) noexcept
        : TrackSymbolInstance{std::move(symbol), Orientation::North, std::move(detail)}
    {}

    TrackSymbolInstance(TrackSymbol symbol, Orientation orientation, QVariant detail = {}) noexcept
        : symbol{std::move(symbol)}
        , orientation{orientation}
        , detail{std::move(detail)}
    {}

    TrackSymbolInstance(QString text, Orientation orientation = Orientation::North) noexcept
        : TrackSymbolInstance{{TrackSymbol::Type::Text, {}, {}, {}}, orientation, std::move(text)}
    {}

    TrackSymbol symbol;
    Orientation orientation = Orientation::North;
    QVariant detail;

    operator QVariant() const { return QVariant::fromValue(*this); }

    std::optional<dcc::AccessoryAddress> turnoutAddress() const;
    std::optional<std::pair<int, int>> turnoutAddressPair() const;
};

class SymbolicTrackPlanModel : public QAbstractTableModel
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged FINAL)

public:
    enum DataRole {
        CellRole = Qt::UserRole,
        TypeRole,
        ModeRole,
        StateRole,
        LinksRole,
        OrientationRole,
        DetailRole,
    };

    Q_ENUM(DataRole)

    enum class Preset {
        Empty,
        SymbolGallery,
        Loops,
        Roofs,
    };

    Q_ENUM(Preset)

    explicit SymbolicTrackPlanModel(QObject *parent = nullptr);

    bool setData(const QModelIndex &index, const QVariant &data, int role = DataRole::CellRole) override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    int rowCount(const QModelIndex &parent = {}) const override;

    void setTitle(QString newTitle);
    QString title() const;

    void resize(int rowCount, int columnCount);
    void resize(QSize size);

    QModelIndexList findAccessories(dcc::AccessoryAddress address) const;
    QModelIndexList findDetectors(const rm::DetectorAddress &address) const;

public slots:
    void reset(lmrs::core::SymbolicTrackPlanModel::Preset preset = Preset::Empty);

signals:
    void titleChanged(QString title);

private:
    class Private;
    Private *const d;
};

struct SymbolicTrackPlanLayout
{
    QString name;
    std::vector<std::unique_ptr<SymbolicTrackPlanModel>> pages;
};

using SymbolicTrackPlanReader = FileFormatReader<SymbolicTrackPlanLayout>;
using SymbolicTrackPlanWriter = FileFormatWriter<SymbolicTrackPlanLayout>;

} // namespace lmrs::core

LMRS_CORE_ENABLE_FLAGS(lmrs::core::TrackSymbol::State);

namespace lmrs::core {

constexpr auto states(TrackSymbol::Type symbol) -> Flags<TrackSymbol::State>
{
    using Type = TrackSymbol::Type;
    using State = TrackSymbol::State;

    switch (symbol)
    {
    case Type::Empty:
    case Type::Straight:
    case Type::Terminal:
    case Type::NarrowCurve:
    case Type::WideCurve:
    case Type::LevelCrossing:
    case Type::BridgeEntry:
    case Type::BridgeStraight:
    case Type::TunnelEntry:
    case Type::TunnelStraight:
    case Type::TunnelCurve:
    case Type::Platform:
    case Type::RedRoof:
    case Type::BlackRoof:
    case Type::BrownRoof:
    case Type::Trees:
    case Type::Text:
        break;

    case Type::Cross:
        return State::Undefined | State::Straight | State::Crossing;

    case Type::LeftHandPoint:
    case Type::RightHandPoint:
    case Type::DoubleCrossPointSimple:
        return State::Undefined | State::Straight | State::Branched;
    case Type::YPoint:
        return State::Undefined | State::Left | State::Right;
    case Type::ThreeWayPoint:
        return State::Undefined | State::Left | State::Right | State::Straight;
    case Type::SingleCrossPoint:
        return State::Undefined | State::Straight | State::Branched | State::Crossing;

    case Type::DoubleCrossPoint:
        return State::Undefined | State::Straight | State::Crossing | State::Left | State::Right;
    case Type::TwoLightsSignal:
        return State::Undefined | State::Green | State::Red;
    case Type::ThreeLightsSignal:
        return State::Undefined | State::Green | State::Red | State::Yellow;
    case Type::FourLightsSignal:
        return State::Undefined | State::Green | State::Red | State::Yellow | State::White;
    case Type::ShuntingSignal:
        return State::Undefined | State::Proceed | State::Stop;
    case Type::SemaphoreSignal:
        return State::Undefined | State::Proceed | State::Slow | State::Stop;

    case Type::Button:
    case Type::Decoupler:
    case Type::Light:
    case Type::Switch:
    case Type::Detector:
        return State::Undefined | State::Active;
    }

    return State::Undefined;
}

constexpr auto defaultState(TrackSymbol::Type symbol) -> TrackSymbol::State
{
    using Type = TrackSymbol::Type;
    using State = TrackSymbol::State;

    switch (symbol) {
    case Type::Empty:
    case Type::Straight:
    case Type::NarrowCurve:
    case Type::WideCurve:
    case Type::Terminal:
    case Type::Cross:
    case Type::LevelCrossing:
    case Type::BridgeEntry:
    case Type::BridgeStraight:
    case Type::TunnelEntry:
    case Type::TunnelStraight:
    case Type::TunnelCurve:
    case Type::Button:
    case Type::Decoupler:
    case Type::Light:
    case Type::Switch:
    case Type::Detector:
    case Type::Platform:
    case Type::RedRoof:
    case Type::BlackRoof:
    case Type::BrownRoof:
    case Type::Trees:
    case Type::Text:
        break;

    case Type::LeftHandPoint:
    case Type::RightHandPoint:
    case Type::ThreeWayPoint:
    case Type::SingleCrossPoint:
    case Type::DoubleCrossPoint:
    case Type::DoubleCrossPointSimple:
        return State::Straight;

    case Type::YPoint:
        return State::Left;

    case Type::TwoLightsSignal:
    case Type::ThreeLightsSignal:
    case Type::FourLightsSignal:
        return State::Green;

    case Type::ShuntingSignal:
    case Type::SemaphoreSignal:
        return State::Proceed;
    };

    return State::Undefined;
}

constexpr auto labels(TrackSymbol::Type symbol) -> Flags<TrackSymbol::State>
{
    using Type = TrackSymbol::Type;
    using State = TrackSymbol::State;

    switch (symbol)
    {
    case Type::Empty:
    case Type::Terminal:
    case Type::NarrowCurve:
    case Type::WideCurve:
    case Type::Cross:
    case Type::LevelCrossing:
    case Type::BridgeEntry:
    case Type::BridgeStraight:
    case Type::TunnelEntry:
    case Type::TunnelStraight:
    case Type::TunnelCurve:
    case Type::Platform:
    case Type::RedRoof:
    case Type::BlackRoof:
    case Type::BrownRoof:
    case Type::Trees:
    case Type::Text:
        break;

    case Type::LeftHandPoint:
    case Type::RightHandPoint:
    case Type::YPoint:
    case Type::SingleCrossPoint:
        return State::Undefined;

    case Type::DoubleCrossPointSimple:
        return State::Left;

    case Type::ThreeWayPoint:
    case Type::DoubleCrossPoint:
        return State::Left | State::Right;

    case Type::Straight:
    case Type::TwoLightsSignal:
    case Type::ShuntingSignal:
    case Type::Button:
    case Type::Decoupler:
    case Type::Light:
    case Type::Switch:
    case Type::Detector:
        return State::Primary;

    case Type::ThreeLightsSignal:
    case Type::FourLightsSignal:
    case Type::SemaphoreSignal:
        return State::Primary | State::Secondary;
    }

    return {};
}

constexpr auto hasLinks(TrackSymbol::Type symbol)
{
    return symbol == TrackSymbol::Type::Platform
            || symbol == TrackSymbol::Type::RedRoof
            || symbol == TrackSymbol::Type::BlackRoof
            || symbol == TrackSymbol::Type::BrownRoof;
}

constexpr auto turnoutState(TrackSymbol::State state) -> dcc::TurnoutState
{
    using State = TrackSymbol::State;

    switch (state) {
    case State::Straight:
    case State::Left:
        return dcc::TurnoutState::Straight;
    case State::Branched:
    case State::Right:
        return dcc::TurnoutState::Branched;

    case State::Crossing:
        break; // FIXME: primaryTurnoutState() / secondaryTurnoutState()

    case State::Undefined:
    case State::Active:
    case State::Primary:
    case State::Secondary:
    case State::Green:
    case State::Red:
    case State::Yellow:
    case State::White:
    case State::Proceed:
    case State::Slow:
    case State::Stop:
        break;
    }

    return dcc::TurnoutState::Unknown;
}

} // namespace lmrs::core

template<>
struct std::hash<lmrs::core::TrackSymbol>
{
    constexpr size_t operator()(const lmrs::core::TrackSymbol &symbol, size_t seed = 0) const noexcept
    {
        return qHashMulti(seed, value(symbol.type), value(symbol.mode), value(symbol.state));
    }
};

#endif // LMRS_CORE_SYMBOLICTRACKPLANMODEL_H
