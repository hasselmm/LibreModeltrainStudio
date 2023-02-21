#include "functiontablemodel.h"

#include "metadata.h"

#include <QMetaEnum>

namespace RailComPlus {

Q_NAMESPACE

enum class Function {
    RandomSound,
    GenericFunction = 2,
    Headlight = 3,
    InteriorLight = 4,
    Cablight = 5,
    DriveSound = 6,
    GenericSound = 7,
    StationAnnouncement = 8,
    RoutingSpeed = 9,
    StartStopDelay = 10,
    AutomaticCoupler = 11,
    SmokeUnit = 12,
    Pantograph = 13,
    Highbeam = 14,
    Bell = 15,
    Horn = 16,
    Whistle = 17,
    Doors = 18,
    Fan = 19,
    ShovelWork = 20,
    Shift = 21,
    PlateLight = 22,
    BrakeSound = 23,
    CraneRaiseLower = 24,
    CraneHookRaiseLower = 25,
    WheelLight = 26,
    CraneTurn = 27,
    SteamBlow = 28,
    RadioSound = 29,
    CouplerSound = 30,
    TrackSound = 31,
    NotchUp = 32,
    NotchDown = 33,
    ThundererWhistle = 34,
    BufferSound = 35,
    Cablight2 = 36,
    CatchWater = 37,
    CurveSound = 38,
    TurnoutSound = 39,
    ReliefValve = 40,
    OilBurner = 41,
    Stoker = 42,
    DynamicBrake = 43,
    Compressor = 44,
    AirBlow = 45,
    HandBrake = 46,
    AirPump = 47,
    WaterPump = 48,
    MainCircuitBreaker = 49,
    DitchLight = 50,
    MarsLight = 51,
    GyraLight = 52,
    Rule17 = 53,
    RotaryBeacon = 54,
    Firebox = 55,
    OilCooler = 56,
    Sanding = 57,
    DrainValve = 58,
    SetBrake = 59,
    ShuntingLight = 60,
    BoardLight = 61,
    Injector = 62,
    AuxilaryDiesel = 63,
    RearLightRed = 64,
    Doppler = 65,
    WhistleShort = 66,
    OilPump = 67,
    Heating = 68,
    GeneratorSteam = 69,
    DieselPump = 70,
    DirectonControlSwitch = 71,
    AuxilaryAirCompressor = 72,
    Lubrication = 73,
    SifaMessage = 74,
    RunningNotch = 75,
    HeavyLoad = 76,
    CoastOperation = 77,
    RearLight = 78,
    FrontLight = 79,
    HighbeamRear = 80,
    HighbeamFront = 81,
    TableLight1 = 84,
    StepLights = 86,
    CablightRear = 87,
    CablightFront = 88,
    PantoRear = 89,
    PantoFront = 90,
    AutoCouplerRear = 93,
    AutoCouplerFront = 94,
    CraneLeft = 95,
    CraneRight = 96,
    CraneUp = 97,
    CraneDown = 98,
    CraneLeftRight = 99,
    SoundFaderMute = 101,
    DoubleHorn = 106,
    Party = 107,
    CraneMagnet = 114,
    RefillDiesel = 124,
};

Q_ENUM_NS(Function)

} // namespace RailComPlus

void FunctionTableModel::setMetaData(std::shared_ptr<esu::MetaData> metaData)
{
    if (metaData != m_metaData) {
        beginResetModel();
        std::swap(m_metaData, metaData);
        endResetModel();
    }

}

std::shared_ptr<esu::MetaData> FunctionTableModel::metaData() const
{
    return m_metaData;
}

int FunctionTableModel::rowCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(!m_metaData))
        return 0;
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return static_cast<int>(m_metaData->functions.count());
}

int FunctionTableModel::columnCount(const QModelIndex &parent) const
{
    if (Q_UNLIKELY(parent.isValid()))
        return 0;

    return QMetaEnum::fromType<DataColumn>().keyCount();
}

QVariant FunctionTableModel::data(const QModelIndex &index, int role) const
{
    if (hasIndex(index.row(), index.column(), index.parent())) {
        const auto &function = m_metaData->functions.at(index.row());

        switch (static_cast<DataColumn>(index.column())) {
        case NameColumn:
            if (role == Qt::DisplayRole)
                return tr("F%1").arg(function.id);

            break;

        case DescriptionColumn:
            if (role == Qt::DisplayRole)
                return function.description.toString();

            break;

        case SymbolColumn:
            if (function.config) {
                if (role == Qt::DisplayRole) {
                    if (const auto key = QMetaEnum::fromType<RailComPlus::Function>().valueToKey(function.index()))
                        return QString::fromLatin1(key);

                    return QString::number(function.index());
                } else if (role == Qt::EditRole) {
                    return function.index();
                }
            }

            break;

        case MomentaryColumn:
            if (function.config && role == Qt::CheckStateRole)
                return function.isMomentary() ? Qt::Checked : Qt::Unchecked;

            break;

        case InvertedColumn:
            if (function.config && role == Qt::CheckStateRole)
                return function.isInverted() ? Qt::Checked : Qt::Unchecked;

            break;

        case CategoryColumn:
            if (function.config && role == Qt::DisplayRole) {
                static const auto meta = QMetaEnum::fromType<esu::Function::Category>();
                if (const auto key = meta.valueToKey(static_cast<int>(function.category())))
                    return QString::fromLatin1(key);

                return QString::number(static_cast<int>(function.category()));
            }

            break;
        }
    }

    return {};
}

QVariant FunctionTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        switch (static_cast<DataColumn>(section)) {
        case NameColumn:
            if (role == Qt::DisplayRole)
                return tr("Function");

            break;

        case DescriptionColumn:
            if (role == Qt::DisplayRole)
                return tr("Description");

            break;

        case SymbolColumn:
            if (role == Qt::DisplayRole)
                return tr("Symbol");

            break;

        case MomentaryColumn:
            if (role == Qt::DisplayRole)
                return tr("Momentary");

            break;

        case InvertedColumn:
            if (role == Qt::DisplayRole)
                return tr("Inverted");

            break;

        case CategoryColumn:
            if (role == Qt::DisplayRole)
                return tr("Category");

            break;
        }
    }

    return {};
}

#include "functiontablemodel.moc"
