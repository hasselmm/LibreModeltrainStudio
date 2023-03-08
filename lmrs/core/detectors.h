#ifndef LMRS_CORE_DETECTORS_H
#define LMRS_CORE_DETECTORS_H

#include "dccconstants.h"

#include <QVariant>

namespace lmrs::core::accessory {

namespace can {

using NetworkId = literal<quint16, struct NetworkIdTag>;
using ModuleId  = literal<quint16, struct ModuleIdTag>;
using PortIndex = literal<quint8,  struct PortIndexTag>;

constexpr auto NetworkIdAny = NetworkId{0xd000};

struct ModuleAddress
{
    NetworkId network;
    ModuleId module;

    [[nodiscard]] constexpr auto operator<=>(const ModuleAddress &rhs) const noexcept = default;
};

struct PortAddress
{
    NetworkId network;
    ModuleId module;
    PortIndex port;

    [[nodiscard]] constexpr auto operator<=>(const PortAddress &rhs) const noexcept = default;

    constexpr auto moduleAddress() const { return ModuleAddress{network, module}; }
};

constexpr size_t qHash(const ModuleAddress &address, size_t seed = 0) noexcept
{
    return qHashMulti(seed, address.network, address.module);
}

constexpr size_t qHash(const PortAddress &address, size_t seed = 0) noexcept
{
    return qHashMulti(seed, address.network, address.module, address.port);
}

} // namespace can

namespace lissy {

using FeedbackAddress = core::literal<quint16, struct FeedbackAddressTag>;

} // namespace lissy

namespace loconet {

struct StationaryInterrogate
{
    constexpr auto operator==(const StationaryInterrogate &) const noexcept { return true; }
};

using ReportAddress = core::literal<quint16, struct ReportAddressTag>;

constexpr auto ReportAddressDefault = ReportAddress{1017};

} // namespace loconet

namespace rbus {

constexpr auto ModulesPerGroup = 10;
constexpr auto PortsPerModule = 8;
constexpr auto PortsPerGroup = ModulesPerGroup * PortsPerModule;

using GroupId   = literal<quint8, struct ModuleGroupTag, 0, 1>;
using ModuleId  = literal<quint8, struct ModuleIndexTag, 1, ModulesPerGroup * (GroupId::Maximum + 1)>;
using PortIndex = literal<quint8, struct PortIndexTag,   1, PortsPerModule>;

static_assert(ModuleId::Maximum == 20);
static_assert(PortIndex::Maximum == 8);

struct PortAddress
{
    ModuleId module;
    PortIndex port;

    [[nodiscard]] constexpr auto operator<=>(const PortAddress &rhs) const noexcept = default;
};

constexpr auto group(ModuleId module) noexcept
{
    return GroupId{static_cast<quint8>((module - 1) / ModulesPerGroup)};
}

constexpr size_t qHash(const PortAddress &address, size_t seed = 0) noexcept
{
    return qHashMulti(seed, address.module, address.port);
}

} // namespace rbus

struct DetectorAddress
{
    Q_GADGET
    Q_PROPERTY(Type type READ type CONSTANT FINAL)

    using value_type = std::variant<std::monostate,
                                    can::NetworkId, can::ModuleAddress, can::PortAddress,
                                    lissy::FeedbackAddress,
                                    loconet::StationaryInterrogate, loconet::ReportAddress,
                                    rbus::GroupId, rbus::ModuleId, rbus::PortAddress>;

public:
    enum class Type {
        Invalid,
        CanNetwork,
        CanModule,
        CanPort,
        LissyModule,
        LoconetSIC,     // "Stationary Interrogate Request" (Digitrax, Blücher...)
        LoconetModule,  // e.g. Uhlenbrock UB63320
        RBusGroup,
        RBusModule,
        RBusPort,
    };

    Q_ENUM(Type)

    constexpr DetectorAddress() noexcept = default;

    constexpr auto type() const { return Type{static_cast<int>(m_value.index())}; }

public slots:
    can::NetworkId canNetwork() const;
    can::ModuleId canModule() const;
    can::PortIndex canPort() const;

    can::ModuleAddress canModuleAddress() const;
    can::PortAddress canPortAddress() const;

    lissy::FeedbackAddress lissyModule() const;

    loconet::ReportAddress loconetModule() const;

    rbus::GroupId rbusGroup() const;
    rbus::ModuleId rbusModule() const;
    rbus::PortIndex rbusPort() const;

    rbus::PortAddress rbusPortAddress() const;

    [[nodiscard]] static DetectorAddress forCanNetwork(can::NetworkId network);
    [[nodiscard]] static DetectorAddress forCanModule(can::NetworkId network, can::ModuleId module);
    [[nodiscard]] static DetectorAddress forCanPort(can::NetworkId network, can::ModuleId module, can::PortIndex port);

    [[nodiscard]] static DetectorAddress forLissyModule(lissy::FeedbackAddress address);

    [[nodiscard]] static DetectorAddress forLoconetSIC();
    [[nodiscard]] static DetectorAddress forLoconetModule(loconet::ReportAddress address);

    [[nodiscard]] static DetectorAddress forRBusGroup(rbus::GroupId group);
    [[nodiscard]] static DetectorAddress forRBusModule(rbus::ModuleId module);
    [[nodiscard]] static DetectorAddress forRBusPort(rbus::ModuleId module, rbus::PortIndex port);

public:
    bool operator==(const DetectorAddress &rhs) const noexcept;
    bool operator!=(const DetectorAddress &rhs) const noexcept = default;
    operator QVariant() const { return QVariant::fromValue(*this); }

private:
    constexpr explicit DetectorAddress(value_type value) noexcept
        : m_value{std::move(value)}
    {}

    template<typename T> bool equals(const DetectorAddress &rhs) const;

    static auto &logger();

    value_type m_value;
};

static_assert(DetectorAddress{}.type() == DetectorAddress::Type::Invalid);

QDebug operator<<(QDebug debug, const DetectorAddress &address);

constexpr size_t qHash(const DetectorAddress &address, size_t seed = 0) noexcept
{
    using Type = lmrs::core::accessory::DetectorAddress::Type;

    switch (address.type()) {
    case Type::CanNetwork:
        return qHashMulti(seed, value(address.type()), address.canNetwork());
    case Type::CanModule:
        return qHashMulti(seed, value(address.type()), address.canModuleAddress());
    case Type::CanPort:
        return qHashMulti(seed, value(address.type()), address.canPortAddress());

    case Type::LissyModule:
        return qHashMulti(seed, value(address.type()), address.lissyModule());

    case Type::LoconetModule:
        return qHashMulti(seed, value(address.type()), address.loconetModule());

    case Type::RBusGroup:
        return qHashMulti(seed, value(address.type()), address.rbusGroup());
    case Type::RBusModule:
        return qHashMulti(seed, value(address.type()), address.rbusModule());
    case Type::RBusPort:
        return qHashMulti(seed, value(address.type()), address.rbusPortAddress());

    case Type::LoconetSIC:
    case Type::Invalid:
        break;
    }

    return qHashMulti(seed, value(address.type()));
}

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

    DetectorInfo(DetectorAddress module)
        : m_address{std::move(module)}
    {}

    DetectorInfo(DetectorAddress module, Occupancy occupancy, PowerState powerState) noexcept
        : DetectorInfo{std::move(module), occupancy, powerState, {}, {}}
    {}

    DetectorInfo(DetectorAddress module,
                 Occupancy occupancy, PowerState powerState,
                 QList<dcc::VehicleAddress> vehicles,
                 QList<dcc::Direction> directions) noexcept
        : m_address{std::move(module)}
        , m_occupancy{occupancy}
        , m_powerState{powerState}
        , m_vehicles{std::move(vehicles)}
        , m_directions{std::move(directions)}
    {}

    [[nodiscard]] constexpr auto address() const noexcept { return m_address; }
    [[nodiscard]] constexpr auto occupancy() const noexcept { return m_occupancy;}
    [[nodiscard]] constexpr auto powerState() const noexcept { return m_powerState;}
    [[nodiscard]] auto vehicles() const noexcept { return m_vehicles; }
    [[nodiscard]] auto directions() const noexcept { return m_directions; }

    void setOccupancy(Occupancy occupancy) { m_occupancy = occupancy; }
    void setPowerState(PowerState powerState) { m_powerState = powerState; }
    void addVehicles(QList<dcc::VehicleAddress> vehicles) { m_vehicles += std::move(vehicles); }
    void addDirections(QList<dcc::Direction> directions) { m_directions += std::move(directions); }

    [[nodiscard]] bool operator==(const DetectorInfo &rhs) const noexcept = default;

private:
    DetectorAddress m_address = {};

    Occupancy m_occupancy = Occupancy::Unknown;
    PowerState m_powerState = PowerState::Unknown;
    QList<dcc::VehicleAddress> m_vehicles;
    QList<dcc::Direction> m_directions;
};

QDebug operator<<(QDebug debug, const DetectorInfo &info);

} // namespace lmrs::core::accessory

#endif // LMRS_CORE_DETECTORS_H
