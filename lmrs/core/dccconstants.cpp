#include "dccconstants.h"

#include "logging.h"
#include "userliterals.h"

#include <QMetaEnum>

namespace lmrs::core::dcc {

static_assert(MinimumValue<BasicAddress> == 0);
static_assert(MaximumValue<BasicAddress> == 127);
static_assert(MinimumValue<VehicleAddress> == 1);
static_assert(MaximumValue<VehicleAddress> == 0x27ff);
static_assert(MinimumValue<AccessoryAddress> == 1);
static_assert(MaximumValue<AccessoryAddress> == 0x3fff);

static_assert(MinimumValue<VariableIndex> == 1);
static_assert(MaximumValue<VariableIndex> == 1024);
static_assert(MinimumValue<VariableValue> == 0);
static_assert(MaximumValue<VariableValue> == 255);
static_assert(MinimumValue<ExtendedPageIndex> == 0);
static_assert(MaximumValue<ExtendedPageIndex> == 65535);
static_assert(MinimumValue<ExtendedPageIndex> == 0);
static_assert(MaximumValue<SusiPageIndex> == 255);

static_assert(MinimumValue<Function> == 0);
static_assert(MaximumValue<Function> == 68);

static_assert(range(VariableSpace::Extended).contains(static_cast<VehicleVariable>(0x101)));
static_assert(variableIndex(static_cast<VehicleVariable>(0x101)) == 257);

static_assert(Speed14{0} == SpeedPercentil{0});
static_assert(Speed28{0} == SpeedPercentil{0});
static_assert(Speed126{0} == SpeedPercentil{0});

static_assert(Speed14{8} == SpeedPercentil{50});
static_assert(Speed28{16} == SpeedPercentil{50});
static_assert(Speed126{64} == SpeedPercentil{50});

static_assert(Speed14{15} == SpeedPercentil{100});
static_assert(Speed28{31} == SpeedPercentil{100});
static_assert(Speed126{127} == SpeedPercentil{100});

static_assert(Speed14{15} == Speed126{127});
static_assert(Speed28{31} == Speed126{127});
static_assert(Speed126{127} == Speed126{127});

static_assert(SpeedPercentil::ratio_type::num == 1);
static_assert(SpeedPercentil::ratio_type::den == 100);

static_assert(MinimumValue<Speed14>         == 0);
static_assert(MinimumValue<Speed28>         == 0);
static_assert(MinimumValue<Speed126>        == 0);
static_assert(MinimumValue<SpeedPercentil>  == 0);

static_assert(MaximumValue<Speed14>         == 15);
static_assert(MaximumValue<Speed28>         == 31);
static_assert(MaximumValue<Speed126>        == 127);
static_assert(MaximumValue<SpeedPercentil>  == 100);

static_assert(quantityCast<SpeedPercentil>(Speed14{0}).count() == 0);
static_assert(quantityCast<SpeedPercentil>(Speed28{0}).count() == 0);
static_assert(quantityCast<SpeedPercentil>(Speed126{0}).count() == 0);

static_assert(quantityCast<SpeedPercentil>(Speed14{8}).count() == 53);
static_assert(quantityCast<SpeedPercentil>(Speed28{16}).count() == 52);
static_assert(quantityCast<SpeedPercentil>(Speed126{64}).count() == 50);

static_assert(quantityCast<SpeedPercentil>(Speed14{15}).count() == 100);
static_assert(quantityCast<SpeedPercentil>(Speed28{31}).count() == 100);
static_assert(quantityCast<SpeedPercentil>(Speed126{127}).count() == 100);

static_assert(speedCast<SpeedPercentil>(Speed{}).count() == 0);
static_assert(speedCast<SpeedPercentil>(Speed{Speed14{15}}).count() == 100);
static_assert(speedCast<SpeedPercentil>(Speed{Speed28{31}}).count() == 100);
static_assert(speedCast<SpeedPercentil>(Speed{Speed126{127}}).count() == 100);
static_assert(speedCast<SpeedPercentil>(Speed{SpeedPercentil{100}}).count() == 100);

namespace {

QString subscript(auto number)
{
    if (number == 0)
        return QChar{0x2080};

    QString text;

    for (; number > 0; number /= 10)
        text.prepend(QChar{0x2080 + (number % 10)});

    return text;
}

QString variableSuffix(ExtendedVariableIndex variable)
{
    const auto baseVariable = dcc::variableIndex(variable);

    if (dcc::range(dcc::VariableSpace::Extended).contains(baseVariable))
        return subscript(dcc::extendedPage(variable).value);
    else if (dcc::range(dcc::VariableSpace::Susi).contains(baseVariable))
        return subscript(dcc::susiPage(variable).value);

    return {};
}

} // namespace

QString variableString(ExtendedVariableIndex variable)
{
    const auto baseVariable = dcc::variableIndex(variable);
    return u"CV\u202f"_qs + QString::number(baseVariable) + variableSuffix(variable);
}

QString fullVariableName(ExtendedVariableIndex variable)
{
    static const auto shortNames = std::map<VehicleVariable, QLatin1String> {
        {VehicleVariable::AccelerationAdjustment, "AccelerationAdjust"_L1},
        {VehicleVariable::DecelerationAdjustment, "DecelerationAdjust"_L1},
        {VehicleVariable::ExtendedAddressHigh, "ExtendedAddrHigh"_L1},
        {VehicleVariable::ExtendedAddressLow, "ExtendedAddrLow"_L1},
    };

    static const auto meta = QMetaEnum::fromType<VehicleVariable>();
    static constexpr auto firstSuffix = "Begin"_L1;
    static constexpr auto lastSuffix = "End"_L1;

    QString name;

    // See if this variable directly matches a known name.
    if (const auto it = shortNames.find(vehicleVariable(variable)); it != shortNames.end())
        name = it->second;
    else if (const auto key = meta.valueToKey(static_cast<int>(variable)))
        name = QString::fromLatin1(key);

    // If the name we found describes start or end of a range, we do not want to show it.
    // These variables and variables in the range get special treatment below.
    if (name.endsWith(firstSuffix) || name.endsWith(lastSuffix))
        name.clear();

    if (name.isEmpty()) {
        const auto meta = QMetaEnum::fromType<dcc::VariableSpace>();
        const auto baseVariable = dcc::variableIndex(variable);

        for (auto i = meta.keyCount() - 1; i >= 0; --i) {
            const auto space = static_cast<dcc::VariableSpace>(meta.value(i));

            if (const auto range = dcc::range(space); range.contains(variable)) {
                const auto offset = variable - lmrs::core::value(range.first);
                name = QString::fromLatin1(meta.key(i));

                if (name.back().isDigit())
                    name += '.'_L1;

                name += QString::number(offset);
                break;
            } else if (range.contains(baseVariable)) {
                const auto offset = baseVariable - lmrs::core::value(range.first);
                name = QString::fromLatin1(meta.key(i));

                if (name.back().isDigit())
                    name += '.'_L1;

                name += QString::number(offset);
                name += variableSuffix(variable);
                break;
            }
        }
    }

    return name;
}

quint8 functionMask(core::Range<Function> group, FunctionState state)
{
    auto mask = 0_u8;

    for (auto fn = value(group.first); fn <= group.last; ++fn) {
        if (state[fn]) {
            if (fn == 0)
                mask |= 0x10_u8;
            else if (fn >= 1 && fn <= 4)
                mask |= static_cast<quint8>(1 << (fn - 1));
            else
                mask |= static_cast<quint8>(1 << (fn - group.first));
        }
    }

    return mask;
}

template<quint8 Steps>
QDebug operator<<(QDebug debug, const SpeedQuantity<Steps> &speed)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(speed)>{debug};
    return debug << speed.count() << '/' << Steps;
}

QDebug operator<<(QDebug debug, Speed speed)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(speed)>{debug};

    if (std::holds_alternative<Speed14>(speed))
        return debug << std::get<Speed14>(speed).count() << "/14";
    if (std::holds_alternative<Speed28>(speed))
        return debug << std::get<Speed28>(speed).count() << "/28";
    if (std::holds_alternative<Speed126>(speed))
        return debug << std::get<Speed126>(speed).count() << "/126";
    if (std::holds_alternative<SpeedPercentil>(speed))
        return debug << std::get<SpeedPercentil>(speed).count() << "/100";

    return debug << "invalid";
}

} // namespace lmrs::core::dcc

namespace lmrs::core::rm {

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

bool DetectorAddress::operator==(const DetectorAddress &rhs) const
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

} // namespace lmrs::core::rm

using namespace lmrs::core;

QDebug operator<<(QDebug debug, const dcc::FunctionState &functions)
{
    const auto prettyPrinter = PrettyPrinter<decltype(functions)>{debug};
    auto separatorNeeded = false;

    for (auto i = 0U; i < functions.size(); ++i) {
        if (functions[i]) {
            if (separatorNeeded)
                debug << ", ";
            else
                separatorNeeded = true;

            debug << 'F' << i;
        }
    }

    return debug;
}
