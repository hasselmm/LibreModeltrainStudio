#ifndef LMRS_CORE_MODEL_H
#define LMRS_CORE_MODEL_H

#include "dccconstants.h"

namespace lmrs::core {

struct AccessoryInfo
{
    Q_GADGET
    Q_PROPERTY(lmrs::core::dcc::AccessoryAddress address READ address CONSTANT FINAL)
    Q_PROPERTY(quint8 state READ state CONSTANT FINAL) // FIXME: How to have dcc::AccessoryState as type?

public:
    enum class CommonSignal {
        Halt            = 0,
        Go              = 0x10,
        StopEmred       = 0x40,
        ShuntGo         = 0x41,
        SignalDark      = 0x42,
        Kennlicht       = 0x43,
        Joker           = 0x44,
        Substitution    = 0x45,
        Counterline     = 0x46,
        Cautiousness    = 0x47,
    };

    Q_ENUM(CommonSignal)

    constexpr AccessoryInfo(dcc::AccessoryAddress address, dcc::AccessoryState state) noexcept
        : m_address(address)
        , m_state(state)
    {}

    constexpr auto address() const noexcept { return m_address; }
    constexpr auto state() const noexcept { return m_state; }

private:
    dcc::AccessoryAddress m_address;
    dcc::AccessoryState m_state;
};

struct DetectorInfo
{
    Q_GADGET

public:
    enum class Occupancy {
        Unknown,
        Free,
        Occupied,
        Invalid,
    };

    Q_ENUM(Occupancy)

    enum class PowerState {
        Unknown,
        Off,
        On,
        Overload,
    };

    Q_ENUM(PowerState)

    DetectorInfo() = default;

    DetectorInfo(rm::DetectorAddress module)
        : m_address{std::move(module)}
    {}

    DetectorInfo(rm::DetectorAddress module, Occupancy occupancy, PowerState powerState) noexcept
        : DetectorInfo{std::move(module), occupancy, powerState, {}, {}}
    {}

    DetectorInfo(rm::DetectorAddress module,
                 Occupancy occupancy, PowerState powerState,
                 QList<dcc::VehicleAddress> vehicles,
                 QList<dcc::Direction> directions) noexcept
        : m_address{std::move(module)}
        , m_occupancy{occupancy}
        , m_powerState{powerState}
        , m_vehicles{std::move(vehicles)}
        , m_directions{std::move(directions)}
    {}

    constexpr auto address() const noexcept { return m_address; }
    constexpr auto occupancy() const noexcept { return m_occupancy;}
    constexpr auto powerState() const noexcept { return m_powerState;}
    auto vehicles() const noexcept { return m_vehicles; }
    auto directions() const noexcept { return m_directions; }

    void setOccupancy(Occupancy occupancy) { m_occupancy = occupancy; }
    void setPowerState(PowerState powerState) { m_powerState = powerState; }
    void addVehicles(QList<dcc::VehicleAddress> vehicles) { m_vehicles += std::move(vehicles); }
    void addDirections(QList<dcc::Direction> directions) { m_directions += std::move(directions); }

    auto fields() const { return std::tie(m_address, m_occupancy, m_powerState, m_vehicles, m_directions); }
    auto operator==(const DetectorInfo &rhs) const { return fields() == rhs.fields(); }
    auto operator!=(const DetectorInfo &rhs) const { return fields() != rhs.fields(); }

private:
    rm::DetectorAddress m_address = {};

    Occupancy m_occupancy = Occupancy::Unknown;
    PowerState m_powerState = PowerState::Unknown;
    QList<dcc::VehicleAddress> m_vehicles;
    QList<dcc::Direction> m_directions;
};

struct TurnoutInfo
{
    Q_GADGET
    Q_PROPERTY(lmrs::core::dcc::AccessoryAddress address READ address CONSTANT FINAL)
    Q_PROPERTY(lmrs::core::dcc::TurnoutState state READ state CONSTANT FINAL)

public:
    constexpr TurnoutInfo(dcc::AccessoryAddress address, dcc::TurnoutState state) noexcept
        : m_address(address)
        , m_state(state)
    {}

    constexpr auto address() const noexcept { return m_address; }
    constexpr auto state() const noexcept { return m_state; }

private:
    dcc::AccessoryAddress m_address;
    dcc::TurnoutState m_state;
};

struct VehicleInfo
{
    Q_GADGET

public:
    enum class Flag {
        IsClaimed = (1 << 0),
        IsConsist = (1 << 1),
    };

    Q_FLAG(Flag)
    Q_DECLARE_FLAGS(Flags, Flag)

    VehicleInfo() = default;

    constexpr explicit VehicleInfo(dcc::VehicleAddress address, dcc::Direction direction, dcc::Speed speed,
                                   dcc::FunctionState functionState = {}, Flags flags = {}) noexcept
        : m_address{address}, m_direction{direction}, m_speed{std::move(speed)}
        , m_functionState{std::move(functionState)}, m_flags{flags}
    {}

    constexpr auto address() const noexcept { return m_address; }
    constexpr auto direction() const noexcept { return m_direction; }
    constexpr auto speed() const noexcept { return m_speed; }
    constexpr auto flags() const noexcept { return Flags{m_flags}; }

    constexpr bool functionState(dcc::Function function) const noexcept { return m_functionState[function]; }
    constexpr auto functionState() const noexcept { return m_functionState; }

private:
    dcc::VehicleAddress m_address = 0;
    dcc::Direction m_direction = dcc::Direction::Forward;
    dcc::Speed m_speed;
    dcc::FunctionState m_functionState;
    Flags::Int m_flags;
};

QDebug operator<<(QDebug debug, const AccessoryInfo &info);
QDebug operator<<(QDebug debug, const DetectorInfo &info);
QDebug operator<<(QDebug debug, const TurnoutInfo &info);
QDebug operator<<(QDebug debug, const VehicleInfo &info);

} // namespace lmrs::core

#endif // LMRS_CORE_MODEL_H
