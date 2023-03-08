#include "detectors.h"

#include "logging.h"

namespace lmrs::core::accessory {

// =====================================================================================================================

can::NetworkId DetectorAddress::canNetwork() const
{
    switch (type()) {
    case Type::CanNetwork:
        return std::get<can::NetworkId>(m_value);
    case Type::CanModule:
        return std::get<can::ModuleAddress>(m_value).network;
    case Type::CanPort:
        return std::get<can::PortAddress>(m_value).network;

    case Type::Invalid:
    case Type::LoconetSIC:
    case Type::LoconetModule:
    case Type::LissyModule:
    case Type::RBusGroup:
    case Type::RBusModule:
    case Type::RBusPort:
        return {};
    }

    Q_UNREACHABLE();
}

can::ModuleId DetectorAddress::canModule() const
{
    return canModuleAddress().module;
}

can::PortIndex DetectorAddress::canPort() const
{
    return canPortAddress().port;
}

can::ModuleAddress DetectorAddress::canModuleAddress() const
{
    switch (type()) {
    case Type::CanModule:
        return std::get<can::ModuleAddress>(m_value);
    case Type::CanPort:
        return std::get<can::PortAddress>(m_value).moduleAddress();

    case Type::Invalid:
    case Type::CanNetwork:
    case Type::LoconetSIC:
    case Type::LoconetModule:
    case Type::LissyModule:
    case Type::RBusGroup:
    case Type::RBusModule:
    case Type::RBusPort:
        return {};
    }

    Q_UNREACHABLE();
}

can::PortAddress DetectorAddress::canPortAddress() const
{
    switch (type()) {
    case Type::CanPort:
        return std::get<can::PortAddress>(m_value);

    case Type::Invalid:
    case Type::CanNetwork:
    case Type::CanModule:
    case Type::LoconetSIC:
    case Type::LoconetModule:
    case Type::LissyModule:
    case Type::RBusGroup:
    case Type::RBusModule:
    case Type::RBusPort:
        return {};
    }

    Q_UNREACHABLE();
}

lissy::FeedbackAddress DetectorAddress::lissyModule() const
{
    switch (type()) {
    case Type::LissyModule:
        return std::get<lissy::FeedbackAddress>(m_value);

    case Type::Invalid:
    case Type::CanNetwork:
    case Type::CanModule:
    case Type::CanPort:
    case Type::LoconetSIC:
    case Type::LoconetModule:
    case Type::RBusGroup:
    case Type::RBusModule:
    case Type::RBusPort:
        return {};
    }

    Q_UNREACHABLE();
}

loconet::ReportAddress DetectorAddress::loconetModule() const
{
    switch (type()) {
    case Type::LoconetModule:
        return std::get<loconet::ReportAddress>(m_value);

    case Type::Invalid:
    case Type::CanNetwork:
    case Type::CanModule:
    case Type::CanPort:
    case Type::LissyModule:
    case Type::LoconetSIC:
    case Type::RBusGroup:
    case Type::RBusModule:
    case Type::RBusPort:
        return {};
    }

    Q_UNREACHABLE();
}

rbus::GroupId DetectorAddress::rbusGroup() const
{
    switch (type()) {
    case Type::RBusGroup:
        return std::get<rbus::GroupId>(m_value);
    case Type::RBusModule:
        return group(std::get<rbus::ModuleId>(m_value));
    case Type::RBusPort:
        return group(std::get<rbus::PortAddress>(m_value).module);

    case Type::Invalid:
    case Type::CanNetwork:
    case Type::CanModule:
    case Type::CanPort:
    case Type::LoconetSIC:
    case Type::LoconetModule:
    case Type::LissyModule:
        return {};
    }

    Q_UNREACHABLE();
}

rbus::ModuleId DetectorAddress::rbusModule() const
{
    switch (type()) {
    case Type::RBusModule:
        return std::get<rbus::ModuleId>(m_value);
    case Type::RBusPort:
        return std::get<rbus::PortAddress>(m_value).module;

    case Type::Invalid:
    case Type::CanNetwork:
    case Type::CanModule:
    case Type::CanPort:
    case Type::LoconetSIC:
    case Type::LoconetModule:
    case Type::LissyModule:
    case Type::RBusGroup:
        return {};
    }

    Q_UNREACHABLE();
}

rbus::PortIndex DetectorAddress::rbusPort() const
{
    return rbusPortAddress().port;
}

rbus::PortAddress DetectorAddress::rbusPortAddress() const
{
    switch (type()) {
    case Type::RBusPort:
        return std::get<rbus::PortAddress>(m_value);

    case Type::Invalid:
    case Type::CanNetwork:
    case Type::CanModule:
    case Type::CanPort:
    case Type::LoconetSIC:
    case Type::LoconetModule:
    case Type::LissyModule:
    case Type::RBusGroup:
    case Type::RBusModule:
        return {};
    }

    Q_UNREACHABLE();
}

// ---------------------------------------------------------------------------------------------------------------------

auto &DetectorAddress::logger()
{
    return core::logger<DetectorAddress>();
}

DetectorAddress DetectorAddress::forCanNetwork(can::NetworkId network)
{
    if (LMRS_FAILED(logger(), inRange<can::NetworkId>(network)))
        return {};

    return DetectorAddress{network};
}

DetectorAddress DetectorAddress::forCanModule(can::NetworkId network, can::ModuleId module)
{
    if (LMRS_FAILED(logger(), inRange<can::NetworkId>(network)))
        return {};
    if (LMRS_FAILED(logger(), inRange<can::ModuleId>(module)))
        return {};

    return DetectorAddress{can::ModuleAddress{network, module}};
}

DetectorAddress DetectorAddress::forCanPort(can::NetworkId network, can::ModuleId module, can::PortIndex port)
{
    if (LMRS_FAILED(logger(), inRange<can::NetworkId>(network)))
        return {};
    if (LMRS_FAILED(logger(), inRange<can::ModuleId>(module)))
        return {};
    if (LMRS_FAILED(logger(), inRange<can::PortIndex>(port)))
        return {};

    auto portAddress = can::PortAddress{network, module, port};
    return DetectorAddress{std::move(portAddress)};
}

DetectorAddress DetectorAddress::forLissyModule(lissy::FeedbackAddress address)
{
    if (LMRS_FAILED(logger(), inRange<lissy::FeedbackAddress>(address)))
        return {};

    return DetectorAddress{address};
}

DetectorAddress DetectorAddress::forLoconetSIC()
{
    return DetectorAddress{loconet::StationaryInterrogate{}};
}

DetectorAddress DetectorAddress::forLoconetModule(loconet::ReportAddress address)
{
    if (LMRS_FAILED(logger(), inRange<loconet::ReportAddress>(address)))
        return {};

    return DetectorAddress{address};
}

DetectorAddress DetectorAddress::forRBusGroup(rbus::GroupId group)
{
    if (LMRS_FAILED(logger(), inRange<rbus::GroupId>(group)))
        return {};

    return DetectorAddress{group};
}

DetectorAddress DetectorAddress::forRBusModule(rbus::ModuleId module)
{
    if (LMRS_FAILED(logger(), inRange<rbus::ModuleId>(module)))
        return {};

    return DetectorAddress{module};
}

DetectorAddress DetectorAddress::forRBusPort(rbus::ModuleId module, rbus::PortIndex port)
{
    if (LMRS_FAILED(logger(), inRange<rbus::ModuleId>(module)))
        return {};
    if (LMRS_FAILED(logger(), inRange<rbus::PortIndex>(port)))
        return {};

    return DetectorAddress{rbus::PortAddress{module, port}};
}

// ---------------------------------------------------------------------------------------------------------------------

template<typename T>
bool DetectorAddress::equals(const DetectorAddress &rhs) const
{
    return std::get<T>(m_value) == std::get<T>(rhs.m_value);
}

bool DetectorAddress::operator==(const DetectorAddress &rhs) const noexcept
{
    if (type() != rhs.type())
        return false;

    switch (type()) {
    case Type::Invalid:
        return equals<std::monostate>(rhs);

    case Type::CanNetwork:
        return equals<can::NetworkId>(rhs);
    case Type::CanModule:
        return equals<can::ModuleAddress>(rhs);
    case Type::CanPort:
        return equals<can::PortAddress>(rhs);

    case Type::LissyModule:
        return equals<lissy::FeedbackAddress>(rhs);

    case Type::LoconetSIC:
        return equals<loconet::StationaryInterrogate>(rhs);
    case Type::LoconetModule:
        return equals<loconet::ReportAddress>(rhs);

    case Type::RBusGroup:
        return equals<rbus::GroupId>(rhs);
    case Type::RBusModule:
        return equals<rbus::ModuleId>(rhs);
    case Type::RBusPort:
        return equals<rbus::PortAddress>(rhs);

    }

    Q_UNREACHABLE();
}

// ---------------------------------------------------------------------------------------------------------------------

QDebug operator<<(QDebug debug, const DetectorAddress &address)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(address)>{debug};
    using Type = DetectorAddress::Type;

    debug.setVerbosity(0);
    debug << "type=" << address.type();


    switch (address.type()) {
    case Type::CanNetwork:
        debug << ", network=0x" << Qt::hex << address.canNetwork();
        break;

    case Type::CanModule:
        debug << ", network=0x" << Qt::hex << address.canNetwork()
              << ", module=" << Qt::dec << address.canModule();
        break;

    case Type::CanPort:
        debug << ", network=0x" << Qt::hex << address.canNetwork()
              << ", module=" << Qt::dec << address.canModule()
              << ", port=" << Qt::dec << address.canPort();
        break;

    case DetectorAddress::Type::LissyModule:
        debug << ", module=" << address.lissyModule();
        break;

    case DetectorAddress::Type::LoconetModule:
        debug << ", module=" << address.loconetModule();
        break;

    case Type::RBusGroup:
        debug << ", group=" << Qt::dec << address.rbusGroup();
        break;

    case Type::RBusModule:
        debug << ", module=" << Qt::dec << address.rbusModule();
        break;

    case Type::RBusPort:
        debug << ", module=" << Qt::dec << address.rbusModule()
              << ", port=" << Qt::dec << address.rbusPort();
        break;

    case DetectorAddress::Type::LoconetSIC:
    case DetectorAddress::Type::Invalid:
        break;
    }

    return debug;
}

QDebug operator<<(QDebug debug, const DetectorInfo &info)
{
    const auto prettyPrinter = PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "module=" << info.address()
          << ", occupancy=" << info.occupancy()
          << ", powerState=" << info.powerState()
          << ", vehicles=" << info.vehicles()
          << ", directions=" << info.directions();

    return debug;
}

} // namespace lmrs::core::accessory
