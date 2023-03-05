#include "symbolictrackplanmodel.h"

#include "algorithms.h"
#include "dccconstants.h"
#include "detectors.h"
#include "fileformat.h"
#include "logging.h"

#include <QSize>

namespace lmrs::core {

// verify flags have been enabled properly

static_assert(flags(lmrs::core::TrackSymbol::State::Undefined) == 1);
static_assert(flags(lmrs::core::TrackSymbol::State::Primary) == 2);
static_assert(flags(lmrs::core::TrackSymbol::State::Secondary) == 4);

static_assert(EnumType<TrackSymbol::State>);
static_assert(EnableFlags<TrackSymbol::State>::value);

static_assert((TrackSymbol::State::Primary | TrackSymbol::State::Secondary) == 6);
static_assert((TrackSymbol::State::Primary | TrackSymbol::State::Secondary | TrackSymbol::State::Undefined) == 7);

// verify assigning states is working

static_assert(states(TrackSymbol::Type::Empty) == 1);
static_assert(states(TrackSymbol::Type::LeftHandPoint) == 25);

namespace {

class FallbackFileReader : public SymbolicTrackPlanReader
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    using SymbolicTrackPlanReader::SymbolicTrackPlanReader;

    ModelPointer read() override;
};

class DelimiterSeparatedFileReader : public SymbolicTrackPlanReader
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    explicit DelimiterSeparatedFileReader(QIODevice *device, char delimiter)
        : SymbolicTrackPlanReader{device}
        , m_delimiter{delimiter}
    {}

    explicit DelimiterSeparatedFileReader(QString fileName, char delimiter)
        : SymbolicTrackPlanReader{std::move(fileName)}
        , m_delimiter{delimiter}
    {}

    ModelPointer read() override;

private:
    char m_delimiter;
};

class DelimiterSeparatedFileWriter : public SymbolicTrackPlanWriter
{
    Q_GADGET
    QT_TR_FUNCTIONS

public:
    explicit DelimiterSeparatedFileWriter(QIODevice *device, char delimiter)
        : SymbolicTrackPlanWriter{device}
        , m_delimiter{delimiter}
    {}

    explicit DelimiterSeparatedFileWriter(QString fileName, char delimiter)
        : SymbolicTrackPlanWriter{std::move(fileName)}
        , m_delimiter{delimiter}
    {}

    bool write(const SymbolicTrackPlanLayout *layout) override;

private:
    QString m_fileName;
    char m_delimiter;
};

FallbackFileReader::ModelPointer FallbackFileReader::read()
{
    reportError(tr("The type of this file is not recognized, or it is not supported at all."));
    return {};
}

DelimiterSeparatedFileReader::ModelPointer DelimiterSeparatedFileReader::read()
{
    reportError(tr("Not implemented yet"));
    return {};
}

bool DelimiterSeparatedFileWriter::write(const SymbolicTrackPlanLayout *)
{
    reportError(tr("Not implemented yet"));
    return false;
}

template<class Handler, char separator>
typename Handler::Factory makeFactory()
{
    return {
        [](QIODevice *device) { return std::make_unique<Handler>(device, separator); },
        [](QString fileName) { return std::make_unique<Handler>(std::move(fileName), separator); },
    };
}

template<typename T>
auto fileFormats(const QList<QPair<FileFormat, T>> &registry)
{
    auto formats = QList<FileFormat>{};
    formats.reserve(registry.size());
    std::transform(registry.begin(), registry.end(), std::back_inserter(formats),
                   [](const auto &entry) { return entry.first; });
    return formats;
}

} // namespace

std::optional<dcc::AccessoryAddress> TrackSymbolInstance::turnoutAddress() const
{
    // FIXME: only return for turnouts
    return get_if<int>(detail);
}

class SymbolicTrackPlanModel::Private : public PrivateObject<SymbolicTrackPlanModel>
{
public:
    using PrivateObject::PrivateObject;

    [[nodiscard]] auto offset(int row, int column) const noexcept { return row * m_columnCount + column; }
    [[nodiscard]] auto offset(const QModelIndex &index) const noexcept { return offset(index.row(), index.column()); }
    [[nodiscard]] auto column(int offset) const noexcept { return m_columnCount > 0 ? offset % m_columnCount : 0; }
    [[nodiscard]] auto column(qsizetype offset) const noexcept { return column(static_cast<int>(offset)); }
    [[nodiscard]] auto row(int offset) const noexcept { return m_columnCount > 0 ? offset / m_columnCount : 0; }
    [[nodiscard]] auto row(qsizetype offset) const noexcept { return row(static_cast<int>(offset)); }
    [[nodiscard]] auto index(qsizetype offset) const { return q()->index(row(offset), column(offset)); }

    void generateSymbolGallery();
    void generateLoops();
    void generateRoofs();

    QList<TrackSymbolInstance> m_cells;
    int m_columnCount = 0;
    QString m_title;
};

SymbolicTrackPlanModel::SymbolicTrackPlanModel(QObject *parent)
    : QAbstractTableModel{parent}
    , d{new Private{this}}
{}

int SymbolicTrackPlanModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return d->m_columnCount;
}

int SymbolicTrackPlanModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    if (d->m_columnCount <= 0)
        return 0;

    return static_cast<int>(d->m_cells.count()) / d->m_columnCount;
}

void SymbolicTrackPlanModel::setTitle(QString newTitle)
{
    if (const auto oldTitle = std::exchange(d->m_title, newTitle); oldTitle != newTitle)
        emit titleChanged(std::move(newTitle));
}

QString SymbolicTrackPlanModel::title() const
{
    return d->m_title;
}

void SymbolicTrackPlanModel::resize(int rowCount, int columnCount)
{
    // FIXME: preserve content

    beginResetModel();
    d->m_columnCount = columnCount;
    d->m_cells.fill({}, rowCount * d->m_columnCount);
    endResetModel();
}

void SymbolicTrackPlanModel::resize(QSize size)
{
    resize(size.height(), size.width());
}

QModelIndexList SymbolicTrackPlanModel::findAccessories(dcc::AccessoryAddress address) const
{
    auto indices = QModelIndexList{};

    for (auto i = 0U; i < d->m_cells.size(); ++i) {
        // FIXME: rather put structured type into detail
        if (d->m_cells[i].turnoutAddress() == address)
            indices += d->index(i);
        else if (const auto &detail = d->m_cells[i].detail; !detail.isNull())
            qCInfo(d->logger()) << detail;
    }

    return indices;
}

QModelIndexList SymbolicTrackPlanModel::findDetectors(const accessory::DetectorAddress &address) const
{
    // FIXME: interface needs to mention module and port, sadly

    if (address.type() != accessory::DetectorAddress::Type::RBusPort)
        return {}; // FIXME: support all kinds of modules

    auto indices = QModelIndexList{};

    for (auto i = 0U; i < d->m_cells.size(); ++i) {
        if (d->m_cells[i].symbol.type == TrackSymbol::Type::Detector) {
            const auto detail = d->m_cells[i].detail;

            // FIXME: rather put structured type into detail
            if (const auto list = qvariant_cast<QList<int>>(detail); list.size() == 2) {
                if (list[0] == address.rbusModule()
                        && (list[1]) == address.rbusPort()) // FIXME: this only works for RBus, not generally
                    indices += d->index(i);
            }
        }
    }

    return indices;
}

void SymbolicTrackPlanModel::reset(Preset preset)
{
    beginResetModel();
    d->m_columnCount = 0;
    d->m_cells.clear();

    switch (preset) {
    case Preset::Empty:
        break;

    case Preset::SymbolGallery:
        d->generateSymbolGallery();
        break;

    case Preset::Loops:
        d->generateLoops();
        break;

    case Preset::Roofs:
        d->generateRoofs();
        break;
    }

    endResetModel();
}

bool SymbolicTrackPlanModel::setData(const QModelIndex &index, const QVariant &data, int role)
{
    auto modified = std::optional<bool>{};

    if (hasIndex(index.row(), index.column(), index.parent())) {
        auto &instance = d->m_cells[d->offset(index)];

        switch (static_cast<DataRole>(role)) {
        case DataRole::CellRole:
            modified = core::setData(&instance, data);
            break;

        case DataRole::TypeRole:
            modified = core::setData(&instance.symbol.type, data);
            break;

        case DataRole::ModeRole:
            modified = core::setData(&instance.symbol.mode, data);
            break;

        case DataRole::StateRole:
            modified = core::setData(&instance.symbol.state, data);
            break;

        case DataRole::LinksRole:
            modified = core::setData(&instance.symbol.links, data);
            break;

        case DataRole::OrientationRole:
            modified = core::setData(&instance.orientation, data);
            break;

        case DataRole::DetailRole:
            modified = core::setData(&instance.detail, data);
            break;
        }

        if (!modified.has_value())
            qCWarning(logger(this), "Ignoring data for unsupported role %d", role);
    }

    if (modified.has_value() && modified.value())
        emit dataChanged(index, index, {role});

    return modified.has_value() && modified.value();
}

QVariant SymbolicTrackPlanModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        const auto &instance = d->m_cells[d->offset(index)];

        switch (static_cast<DataRole>(role)) {
        case DataRole::CellRole:
            return QVariant::fromValue(instance);
        case DataRole::TypeRole:
            return QVariant::fromValue(instance.symbol.type);
        case DataRole::ModeRole:
            return QVariant::fromValue(instance.symbol.mode);
        case DataRole::StateRole:
            return QVariant::fromValue(instance.symbol.state);
        case DataRole::LinksRole:
            return QVariant::fromValue(instance.symbol.links);
        case DataRole::OrientationRole:
            return QVariant::fromValue(instance.orientation);
        case DataRole::DetailRole:
            return QVariant::fromValue(instance.detail);
        }
    }

    return {};
}

void SymbolicTrackPlanModel::Private::generateSymbolGallery()
{
    q()->resize(keyCount<TrackSymbolInstance::Orientation>() * 2 + 1,
                keyCount<TrackSymbol::Type>() * 2 + 1);

    for (const auto &symbol: QMetaTypeId<TrackSymbol::Type>{}) {
        auto symbolStates = QVarLengthArray<TrackSymbol::State>{};

        for (const auto state: QMetaTypeId<TrackSymbol::State>{}) {
            if (states(symbol).isSet(state.value()))
                symbolStates += state;
        }

        for (const auto orientation: QMetaTypeId<TrackSymbolInstance::Orientation>{}) {
            const auto column = symbol.index() * 2 + 1;
            const auto row = 2 * orientation.index() + 1;

            const auto links = hasLinks(symbol) ? TrackSymbol::Link::Center : TrackSymbol::Link::None;
            const auto mode = TrackSymbol::Mode{orientation.index() % core::keyCount<TrackSymbol::Mode>()};
            const auto state = symbolStates[orientation.index() % symbolStates.length()];

            const auto d0 = orientation.index() * 3;
            auto detail = QVariant::fromValue(QList{d0 + 1, d0 + 2, d0 + 3});
            m_cells[offset(row, column)] = {{symbol, mode, state, links}, orientation, std::move(detail)};

            if (row == 1)
                m_cells[offset(0, column)] = QString::fromLatin1(symbol.key());

            if (column == 1) {
                m_cells[offset(row, 0)]
                        = QString::fromLatin1(core::key(mode)) + '\n'_L1
                        + QString::fromLatin1(orientation.key()) + '\n'_L1
                        + QString::number(value(orientation.value())) + '\xb0'_L1;
            }

            m_cells[offset(row + 1, column)] = QString::fromLatin1(core::key(state));
        }
    }
}

void SymbolicTrackPlanModel::Private::generateLoops()
{
    q()->resize(16, 24);

    auto lastPoint = 0;
    auto nextAddressPair = [&lastPoint] {
        return QVariant::fromValue(QList{++lastPoint, ++lastPoint});
    };

    using Orientation = TrackSymbolInstance::Orientation;

    // narrow curves

    m_cells[offset(2, 10)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::East};
    m_cells[offset(2, 11)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::South};
    m_cells[offset(3, 11)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::West};
    m_cells[offset(3, 10)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::North};

    m_cells[offset(3, 4)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::SouthEast};
    m_cells[offset(4, 5)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::SouthWest};
    m_cells[offset(5, 4)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::NorthWest};
    m_cells[offset(4, 3)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::NorthEast};

    m_cells[offset(2, 4)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::SouthEast};
    m_cells[offset(3, 5)] = {{TrackSymbol::Type::Straight},    Orientation::SouthEast};
    m_cells[offset(4, 6)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::SouthWest};
    m_cells[offset(5, 5)] = {{TrackSymbol::Type::Straight},    Orientation::SouthWest};
    m_cells[offset(6, 4)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::NorthWest};
    m_cells[offset(5, 3)] = {{TrackSymbol::Type::Straight},    Orientation::NorthWest};
    m_cells[offset(4, 2)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::NorthEast};
    m_cells[offset(3, 3)] = {{TrackSymbol::Type::Straight},    Orientation::NorthEast};

    m_cells[offset(6,  9)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::East};
    m_cells[offset(6, 10)] = {{TrackSymbol::Type::Straight},    Orientation::East};
    m_cells[offset(6, 11)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::South};
    m_cells[offset(7, 11)] = {{TrackSymbol::Type::Straight},    Orientation::South};
    m_cells[offset(8, 11)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::West};
    m_cells[offset(8, 10)] = {{TrackSymbol::Type::Straight},    Orientation::West};
    m_cells[offset(8,  9)] = {{TrackSymbol::Type::NarrowCurve}, Orientation::North};
    m_cells[offset(7,  9)] = {{TrackSymbol::Type::Straight},    Orientation::North};

    // wide curves

    m_cells[offset(1, 3)] = {{TrackSymbol::Type::WideCurve}, Orientation::East};
    m_cells[offset(1, 4)] = {{TrackSymbol::Type::Straight},  Orientation::East};
    m_cells[offset(1, 5)] = {{TrackSymbol::Type::WideCurve}, Orientation::SouthEast};
    m_cells[offset(2, 6)] = {{TrackSymbol::Type::Straight},  Orientation::SouthEast};
    m_cells[offset(3, 7)] = {{TrackSymbol::Type::WideCurve}, Orientation::South};
    m_cells[offset(4, 7)] = {{TrackSymbol::Type::Straight},  Orientation::South};
    m_cells[offset(5, 7)] = {{TrackSymbol::Type::WideCurve}, Orientation::SouthWest};
    m_cells[offset(6, 6)] = {{TrackSymbol::Type::Straight},  Orientation::SouthWest};
    m_cells[offset(7, 5)] = {{TrackSymbol::Type::WideCurve}, Orientation::West};
    m_cells[offset(7, 4)] = {{TrackSymbol::Type::Straight},  Orientation::West};
    m_cells[offset(7, 3)] = {{TrackSymbol::Type::WideCurve}, Orientation::NorthWest};
    m_cells[offset(6, 2)] = {{TrackSymbol::Type::Straight},  Orientation::NorthWest};
    m_cells[offset(5, 1)] = {{TrackSymbol::Type::WideCurve}, Orientation::North};
    m_cells[offset(4, 1)] = {{TrackSymbol::Type::Straight},  Orientation::North};
    m_cells[offset(3, 1)] = {{TrackSymbol::Type::WideCurve}, Orientation::NorthEast};
    m_cells[offset(2, 2)] = {{TrackSymbol::Type::Straight},  Orientation::NorthEast};

    m_cells[offset(1, 10)] = {{TrackSymbol::Type::WideCurve}, Orientation::East};
    m_cells[offset(1, 11)] = {{TrackSymbol::Type::WideCurve}, Orientation::SouthEast};
    m_cells[offset(2, 12)] = {{TrackSymbol::Type::WideCurve}, Orientation::South};
    m_cells[offset(3, 12)] = {{TrackSymbol::Type::WideCurve}, Orientation::SouthWest};
    m_cells[offset(4, 11)] = {{TrackSymbol::Type::WideCurve}, Orientation::West};
    m_cells[offset(4, 10)] = {{TrackSymbol::Type::WideCurve}, Orientation::NorthWest};
    m_cells[offset(3,  9)] = {{TrackSymbol::Type::WideCurve}, Orientation::North};
    m_cells[offset(2,  9)] = {{TrackSymbol::Type::WideCurve}, Orientation::NorthEast};

    // points

    m_cells[offset(1, 15)] = {{TrackSymbol::Type::LeftHandPoint}, Orientation::West, ++lastPoint};
    m_cells[offset(1, 16)] = {{TrackSymbol::Type::LeftHandPoint}, Orientation::NorthWest, ++lastPoint};
    m_cells[offset(2, 17)] = {{TrackSymbol::Type::LeftHandPoint}, Orientation::North, ++lastPoint};
    m_cells[offset(3, 17)] = {{TrackSymbol::Type::LeftHandPoint}, Orientation::NorthEast, ++lastPoint};
    m_cells[offset(4, 16)] = {{TrackSymbol::Type::LeftHandPoint}, Orientation::East, ++lastPoint};
    m_cells[offset(4, 15)] = {{TrackSymbol::Type::LeftHandPoint}, Orientation::SouthEast, ++lastPoint};
    m_cells[offset(3, 14)] = {{TrackSymbol::Type::LeftHandPoint}, Orientation::South, ++lastPoint};
    m_cells[offset(2, 14)] = {{TrackSymbol::Type::LeftHandPoint}, Orientation::SouthWest, ++lastPoint};

    m_cells[offset(1, 20)] = {{TrackSymbol::Type::RightHandPoint}, Orientation::NorthEast, ++lastPoint};
    m_cells[offset(1, 21)] = {{TrackSymbol::Type::RightHandPoint}, Orientation::East, ++lastPoint};
    m_cells[offset(2, 22)] = {{TrackSymbol::Type::RightHandPoint}, Orientation::SouthEast, ++lastPoint};
    m_cells[offset(3, 22)] = {{TrackSymbol::Type::RightHandPoint}, Orientation::South, ++lastPoint};
    m_cells[offset(4, 21)] = {{TrackSymbol::Type::RightHandPoint}, Orientation::SouthWest, ++lastPoint};
    m_cells[offset(4, 20)] = {{TrackSymbol::Type::RightHandPoint}, Orientation::West, ++lastPoint};
    m_cells[offset(3, 19)] = {{TrackSymbol::Type::RightHandPoint}, Orientation::NorthWest, ++lastPoint};
    m_cells[offset(2, 19)] = {{TrackSymbol::Type::RightHandPoint}, Orientation::North, ++lastPoint};

    m_cells[offset(6, 15)] = {{TrackSymbol::Type::YPoint}, Orientation::West, ++lastPoint};
    m_cells[offset(6, 16)] = {{TrackSymbol::Type::YPoint}, Orientation::NorthWest, ++lastPoint};
    m_cells[offset(7, 17)] = {{TrackSymbol::Type::YPoint}, Orientation::North, ++lastPoint};
    m_cells[offset(8, 17)] = {{TrackSymbol::Type::YPoint}, Orientation::NorthEast, ++lastPoint};
    m_cells[offset(9, 16)] = {{TrackSymbol::Type::YPoint}, Orientation::East, ++lastPoint};
    m_cells[offset(9, 15)] = {{TrackSymbol::Type::YPoint}, Orientation::SouthEast, ++lastPoint};
    m_cells[offset(8, 14)] = {{TrackSymbol::Type::YPoint}, Orientation::South, ++lastPoint};
    m_cells[offset(7, 14)] = {{TrackSymbol::Type::YPoint}, Orientation::SouthWest, ++lastPoint};

    m_cells[offset(6, 20)] = {{TrackSymbol::Type::ThreeWayPoint}, Orientation::NorthEast, ++lastPoint};
    m_cells[offset(6, 21)] = {{TrackSymbol::Type::ThreeWayPoint}, Orientation::East, ++lastPoint};
    m_cells[offset(7, 22)] = {{TrackSymbol::Type::ThreeWayPoint}, Orientation::SouthEast, ++lastPoint};
    m_cells[offset(8, 22)] = {{TrackSymbol::Type::ThreeWayPoint}, Orientation::South, ++lastPoint};
    m_cells[offset(9, 21)] = {{TrackSymbol::Type::ThreeWayPoint}, Orientation::SouthWest, ++lastPoint};
    m_cells[offset(9, 20)] = {{TrackSymbol::Type::ThreeWayPoint}, Orientation::West, ++lastPoint};
    m_cells[offset(8, 19)] = {{TrackSymbol::Type::ThreeWayPoint}, Orientation::NorthWest, ++lastPoint};
    m_cells[offset(7, 19)] = {{TrackSymbol::Type::ThreeWayPoint}, Orientation::North, ++lastPoint};

    m_cells[offset(11, 10)] = {{TrackSymbol::Type::Cross}, Orientation::West};
    m_cells[offset(11, 11)] = {{TrackSymbol::Type::Cross}, Orientation::NorthWest};
    m_cells[offset(12, 12)] = {{TrackSymbol::Type::Cross}, Orientation::North};
    m_cells[offset(13, 12)] = {{TrackSymbol::Type::Cross}, Orientation::NorthEast};
    m_cells[offset(14, 11)] = {{TrackSymbol::Type::Cross}, Orientation::East};
    m_cells[offset(14, 10)] = {{TrackSymbol::Type::Cross}, Orientation::SouthEast};
    m_cells[offset(13,  9)] = {{TrackSymbol::Type::Cross}, Orientation::South};
    m_cells[offset(12,  9)] = {{TrackSymbol::Type::Cross}, Orientation::SouthWest};

    m_cells[offset(11, 15)] = {{TrackSymbol::Type::SingleCrossPoint}, Orientation::West, ++lastPoint};
    m_cells[offset(11, 16)] = {{TrackSymbol::Type::SingleCrossPoint}, Orientation::NorthWest, ++lastPoint};
    m_cells[offset(12, 17)] = {{TrackSymbol::Type::SingleCrossPoint}, Orientation::North, ++lastPoint};
    m_cells[offset(13, 17)] = {{TrackSymbol::Type::SingleCrossPoint}, Orientation::NorthEast, ++lastPoint};
    m_cells[offset(14, 16)] = {{TrackSymbol::Type::SingleCrossPoint}, Orientation::East, ++lastPoint};
    m_cells[offset(14, 15)] = {{TrackSymbol::Type::SingleCrossPoint}, Orientation::SouthEast, ++lastPoint};
    m_cells[offset(13, 14)] = {{TrackSymbol::Type::SingleCrossPoint}, Orientation::South, ++lastPoint};
    m_cells[offset(12, 14)] = {{TrackSymbol::Type::SingleCrossPoint}, Orientation::SouthWest, ++lastPoint};

    m_cells[offset(11, 20)] = {{TrackSymbol::Type::DoubleCrossPoint}, Orientation::West, nextAddressPair()};
    m_cells[offset(11, 21)] = {{TrackSymbol::Type::DoubleCrossPoint}, Orientation::NorthWest, nextAddressPair()};
    m_cells[offset(12, 22)] = {{TrackSymbol::Type::DoubleCrossPoint}, Orientation::North, nextAddressPair()};
    m_cells[offset(13, 22)] = {{TrackSymbol::Type::DoubleCrossPoint}, Orientation::NorthEast, nextAddressPair()};
    m_cells[offset(14, 21)] = {{TrackSymbol::Type::DoubleCrossPoint}, Orientation::East, nextAddressPair()};
    m_cells[offset(14, 20)] = {{TrackSymbol::Type::DoubleCrossPoint}, Orientation::SouthEast, nextAddressPair()};
    m_cells[offset(13, 19)] = {{TrackSymbol::Type::DoubleCrossPoint}, Orientation::South, nextAddressPair()};
    m_cells[offset(12, 19)] = {{TrackSymbol::Type::DoubleCrossPoint}, Orientation::SouthWest, nextAddressPair()};
}

void SymbolicTrackPlanModel::Private::generateRoofs()
{
    q()->resize(11, 13);

    const auto types = std::array {
        TrackSymbol::Type::RedRoof,
        TrackSymbol::Type::BlackRoof,
        TrackSymbol::Type::BrownRoof,
        TrackSymbol::Type::Platform,
    };

    for (auto i = 0U; i < types.size(); ++i) {
        const auto base = offset((i / 2) * 5, (i % 2) * 6);

        m_cells[offset(1,  1) + base] = {{types[i], {}, {}, TrackSymbol::Link::Center}};
        m_cells[offset(1,  2) + base] = {{types[i], {}, {}, TrackSymbol::Link::Cross}};
        m_cells[offset(1,  4) + base] = {{types[i], {}, {}, TrackSymbol::Link::EndNorth}};

        m_cells[offset(2,  1) + base] = {{types[i], {}, {}, TrackSymbol::Link::EndWest}};
        m_cells[offset(2,  2) + base] = {{types[i], {}, {}, TrackSymbol::Link::Horizontal}};
        m_cells[offset(2,  3) + base] = {{types[i], {}, {}, TrackSymbol::Link::TeeSouth}};
        m_cells[offset(2,  4) + base] = {{types[i], {}, {}, TrackSymbol::Link::TeeWest}};

        m_cells[offset(3,  1) + base] = {{types[i], {}, {}, TrackSymbol::Link::CornerNorthWest}};
        m_cells[offset(3,  2) + base] = {{types[i], {}, {}, TrackSymbol::Link::CornerNorthEast}};
        m_cells[offset(3,  3) + base] = {{types[i], {}, {}, TrackSymbol::Link::TeeEast}};
        m_cells[offset(3,  4) + base] = {{types[i], {}, {}, TrackSymbol::Link::TeeNorth}};
        m_cells[offset(3,  5) + base] = {{types[i], {}, {}, TrackSymbol::Link::EndEast}};

        m_cells[offset(4,  1) + base] = {{types[i], {}, {}, TrackSymbol::Link::CornerSouthWest}};
        m_cells[offset(4,  2) + base] = {{types[i], {}, {}, TrackSymbol::Link::CornerSouthEast}};
        m_cells[offset(4,  3) + base] = {{types[i], {}, {}, TrackSymbol::Link::EndSouth}};
    }
}

// ---------------------------------------------------------------------------------------------------------------------

template<>
SymbolicTrackPlanReader::Registry &SymbolicTrackPlanReader::registry()
{
    static auto formats = Registry {
        {FileFormat::csv(), makeFactory<DelimiterSeparatedFileReader, ':'>()},
        {FileFormat::tsv(), makeFactory<DelimiterSeparatedFileReader, '\t'>()},
    };

    return formats;
}

template<>
SymbolicTrackPlanWriter::Registry &SymbolicTrackPlanWriter::registry()
{
    static auto formats = Registry {
        {FileFormat::csv(), makeFactory<DelimiterSeparatedFileWriter, ':'>()},
        {FileFormat::tsv(), makeFactory<DelimiterSeparatedFileWriter, '\t'>()},
    };

    return formats;
}

} // namespace lmrs::core

#include "symbolictrackplanmodel.moc"
