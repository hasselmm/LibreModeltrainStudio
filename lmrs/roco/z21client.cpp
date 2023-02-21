#include "z21client.h"

#include <lmrs/core/continuation.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/userliterals.h>

#include <QBitArray>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QPointer>
#include <QTimer>
#include <QTimerEvent>
#include <QUdpSocket>
#include <QVersionNumber>

#include <QtEndian>

namespace lmrs::roco::z21 {

namespace {

using core::callIfDefined;

auto &lcRequests() { struct Requests {}; return core::logger<Client, Requests>(); }
auto &lcStream() { struct Stream {}; return core::logger<Client, Stream>(); }
auto &lcCanDetector() { struct CanDetector {}; return core::logger<Client, CanDetector>(); }

auto makeKey(LoconetDetectorInfo::Query type, quint16 address)
{
    return (static_cast<quint32>(type) << 16) | address;
}

struct ConfigurationVariable {
    quint16 index;
    quint8 value;
};

class Timer
{
public:
    explicit Timer(QObject *context);

    bool isActive() const { return m_context && m_timerId != 0; }
    bool start(std::chrono::milliseconds interval, Qt::TimerType timerType = Qt::CoarseTimer);
    bool stop();

    bool matches(const QTimerEvent *event) const { return event->timerId() == m_timerId; }

private:
    const QPointer<QObject> m_context;
    int m_timerId = 0;
};

Timer::Timer(QObject *context)
    : m_context{context}
{}

bool Timer::start(std::chrono::milliseconds interval, Qt::TimerType timerType)
{
    if (isActive())
        return false;

    if (m_context)
        m_timerId = m_context->startTimer(interval, timerType);

    return isActive();
}

bool Timer::stop()
{
    if (const auto timerId = std::exchange(m_timerId, 0); timerId && m_context) {
        m_context->killTimer(timerId);
        return true;
    }

    return false;
}

int fromBCD(quint8 byte)
{
    return 10 * (byte >> 4) + (byte & 15);
}

void updateChecksum(QByteArray *message)
{
    const auto first = message->begin() + 5;
    const auto last = message->end() - 1;
    quint8 checksum = 0;

    for (auto it = first; it != last; ++it)
        checksum ^= static_cast<quint8>(*it);

    message->back() = static_cast<char>(checksum);
}

} // namespace

VehicleInfo::VehicleInfo(QByteArray data)
    : m_data{std::move(data)}
{}

bool VehicleInfo::isValid() const
{
    return m_data.size() >= 5;
}

dcc::VehicleAddress VehicleInfo::address() const
{
    return qFromBigEndian<quint16>(m_data.constData()) & 0x3fff;
}

VehicleInfo::Protocol VehicleInfo::protocol() const
{
    return static_cast<Protocol>(m_data[2] & 7);
}

dcc::Speed VehicleInfo::speed() const
{
    const auto speedValue = static_cast<quint8>(m_data[3] & 127);

    switch (protocol()) {
    case DCC14:
        return dcc::Speed14{speedValue};
    case DCC28:
        return dcc::Speed28{speedValue};
    case DCC126:
        return dcc::Speed126{speedValue};
    }

    return {};
}

VehicleInfo::Functions VehicleInfo::functions() const
{
    return {qFromLittleEndian<quint32>(m_data.constData() + 4)}; // FIXME: also read F29-F31, DB8
}

bool VehicleInfo::acquired() const
{
    return m_data[2] & 8;
}

dcc::Direction VehicleInfo::direction() const
{
    return m_data[3] & 128 ? dcc::Direction::Forward : dcc::Direction::Reverse;
}

bool VehicleInfo::consistMode() const
{
    return m_data[4] & 64;
}

bool VehicleInfo::smartSearch() const
{
    return m_data[4] & 32;
}

AccessoryInfo::AccessoryInfo(QByteArray data)
    : m_data{std::move(data)}
{}

bool AccessoryInfo::isValid() const
{
    return m_data.size() >= 4 && m_data[3] == 0;
}

dcc::AccessoryAddress AccessoryInfo::address() const
{
    return (qFromBigEndian<quint16>(m_data.constData()) & 0x7ff) + 1;
}

dcc::AccessoryState AccessoryInfo::state() const
{
    return static_cast<quint8>(m_data[2]);
}

RBusDetectorInfo::RBusDetectorInfo(QByteArray data)
    : m_data{std::move(data)}
{}

bool RBusDetectorInfo::isValid() const
{
    return m_data.size() >= 1 + rm::rbus::ModulesPerGroup;
}

rm::rbus::GroupId RBusDetectorInfo::group() const
{
    return static_cast<quint8>(m_data[0]);
}

QBitArray RBusDetectorInfo::occupancy() const
{
    return QBitArray::fromBits(m_data.constData() + 1, rm::rbus::PortsPerGroup);
}

RBusDetectorInfo::operator QList<core::DetectorInfo>() const
{
    auto result = QList<core::DetectorInfo>{};
    result.reserve(rm::rbus::PortsPerGroup);

    auto address = rm::DetectorAddress::forRBusGroup(group());
    const auto powerState = core::DetectorInfo::PowerState::Unknown;
    const auto occupancyState = occupancy();

    for (auto port = 0; port < occupancyState.size(); ++port) {
        const auto occupancy = occupancyState[port] ? core::DetectorInfo::Occupancy::Occupied
                                                    : core::DetectorInfo::Occupancy::Free;

        result.emplaceBack(std::move(address), occupancy, powerState);
    }

    return result;
}

LoconetDetectorInfo::LoconetDetectorInfo(QByteArray data)
    : m_data{std::move(data)}
{}

bool LoconetDetectorInfo::isValid() const
{
    if (m_data.size() > 1) {
        switch (type()) {
        case Type::Occupancy:
        case Type::LissyOccupancy:
            return m_data.size() >= 4;

        case Type::BlockEnter:
        case Type::BlockLeave:
        case Type::LissySpeed:
            return m_data.size() >= 5;

        case Type::LissyAddress:
            return m_data.size() >= 6;
        }
    }

    return false;
}

LoconetDetectorInfo::Type LoconetDetectorInfo::type() const
{
    return static_cast<Type>(m_data[0]);
}

rm::loconet::ReportAddress LoconetDetectorInfo::address() const
{
    return qFromLittleEndian<quint16>(m_data.constData() + 1);
}

LoconetDetectorInfo::Occupancy LoconetDetectorInfo::occupancy() const
{
    switch (type()) {
    case Type::Occupancy:
    case Type::LissyOccupancy:
        if (m_data[1] == 0)
            return Occupancy::Free;
        else if (m_data[1] == 1)
            return Occupancy::Occupied;

        break;

    case Type::BlockEnter:
    case Type::LissySpeed:
    case Type::LissyAddress:
        return Occupancy::Occupied;

    case Type::BlockLeave:
        return Occupancy::Free;
    }

    return Occupancy::Invalid;
}

dcc::VehicleAddress LoconetDetectorInfo::vehicle() const
{
    switch (type()) {
    case Type::BlockEnter:
    case Type::LissySpeed:
    case Type::LissyAddress:
        return qFromLittleEndian<dcc::VehicleAddress>(m_data.constData() + 1);

    case Type::Occupancy:
    case Type::LissyOccupancy:
    case Type::BlockLeave:
        break;
    }

    return {};
}

dcc::Direction LoconetDetectorInfo::direction() const
{
    if (type() == Type::LissyAddress) {
        const auto direction = static_cast<quint8>(m_data[3]) & 0x60;

        if (direction == 0x40)
            return dcc::Direction::Forward;
        else if (direction == 0x60)
            return dcc::Direction::Reverse;
    }

    return dcc::Direction::Unknown;
}

quint8 LoconetDetectorInfo::lissyClass() const
{
    if (type() == Type::LissyAddress)
        return m_data[3] & 0x0f;

    return {};
}

quint16 LoconetDetectorInfo::lissySpeed() const
{
    if (type() == Type::LissySpeed)
        return qFromLittleEndian<quint16>(m_data.constData() + 1);

    return {};
}

QList<core::DetectorInfo> LoconetDetectorInfo::merge(QList<LoconetDetectorInfo> /*infoList*/)
{
    return {};
}

std::pair<LoconetDetectorInfo::Query, quint16> LoconetDetectorInfo::address(const rm::DetectorAddress &address)
{
    switch (address.type()) {
    case rm::DetectorAddress::Type::LoconetSIC:
        return {Query::SIC, 0};
    case rm::DetectorAddress::Type::LoconetModule:
        return {Query::Report, address.loconetModule()};
    case rm::DetectorAddress::Type::LissyModule:
        return {Query::Lissy, address.lissyModule()};

    case rm::DetectorAddress::Type::CanModule:
    case rm::DetectorAddress::Type::CanNetwork:
    case rm::DetectorAddress::Type::CanPort:
    case rm::DetectorAddress::Type::RBusGroup:
    case rm::DetectorAddress::Type::RBusModule:
    case rm::DetectorAddress::Type::RBusPort:
    case rm::DetectorAddress::Type::Invalid:
        break;
    }

    return {Query::Invalid, 0};
}

CanDetectorInfo::CanDetectorInfo(QByteArray data)
    : m_data{std::move(data)}
{}

bool CanDetectorInfo::isValid() const
{
    return m_data.size() >= 10;
}

rm::can::NetworkId CanDetectorInfo::networkId() const
{
    return qFromLittleEndian<quint16>(m_data.constData());
}

rm::can::ModuleId CanDetectorInfo::module() const
{
    return qFromLittleEndian<quint16>(m_data.constData() + 2);
}

rm::can::PortIndex CanDetectorInfo::port() const
{
    return static_cast<quint8>(m_data[4]);
}

CanDetectorInfo::Type CanDetectorInfo::type() const
{
    return static_cast<Type>(m_data[5]);
}

quint16 CanDetectorInfo::value1() const
{
    return qFromLittleEndian<quint16>(m_data.constData() + 6);
}

quint16 CanDetectorInfo::value2() const
{
    return qFromLittleEndian<quint16>(m_data.constData() + 8);
}

CanDetectorInfo::Occupancy CanDetectorInfo::occupancy() const
{
    if (type() == Type::Occupancy) {
        if (value1() & 0x1000)
            return Occupancy::Occupied;
        else
            return Occupancy::Free;
    }

    return Occupancy::Unknown;
}

CanDetectorInfo::PowerState CanDetectorInfo::powerState() const
{
    if (type() == Type::Occupancy) {
        switch ((value1() >> 8) & 7) {
        case 0:
            return PowerState::Off;
        case 1:
            return PowerState::On;
        case 2:
            return PowerState::Overload;
        }
    }

    return PowerState::Unknown;
}

QList<dcc::VehicleAddress> CanDetectorInfo::vehicles() const
{
    auto vehicles = QList<dcc::VehicleAddress>{};

    if (isVehicleSet() && value1() != 0) {
        vehicles += vehicle1();

        if (value2() != 0)
            vehicles += vehicle2();
    }

    return vehicles;
}

QList<dcc::Direction> CanDetectorInfo::directions() const
{
    auto directions = QList<dcc::Direction>{};

    if (isVehicleSet() && value1() != 0) {
        directions += direction1();

        if (value2() != 0)
            directions += direction2();
    }

    return directions;
}

QList<core::DetectorInfo> CanDetectorInfo::merge(QList<CanDetectorInfo> infoList)
{
    struct GenericInfoChunk {
        GenericInfoChunk(CanDetectorInfo::Key key, rm::DetectorAddress address)
            : key{std::move(key)}
            , info{std::move(address)}
        {}

        CanDetectorInfo::Key key;
        core::DetectorInfo info;
    };

    auto genericInfoList = QList<GenericInfoChunk>{};

    for (const auto &info: infoList) {
        auto it = std::find_if(genericInfoList.begin(), genericInfoList.end(),
                               [key = info.key()](const auto &chunk) {
            return chunk.key == key;
        });

        if (it == genericInfoList.end()) {
            auto address = rm::DetectorAddress::forCanPort(info.networkId(), info.module(), info.port());
            it = genericInfoList.emplace(genericInfoList.end(), info.key(), std::move(address));
        }

        switch (info.type()) {
        case CanDetectorInfo::Type::Occupancy:
            it->info.setOccupancy(info.occupancy());
            it->info.setPowerState(info.powerState());
            continue;

        case CanDetectorInfo::Type::VehicleSet1:
        case CanDetectorInfo::Type::VehicleSet2:
        case CanDetectorInfo::Type::VehicleSet3:
        case CanDetectorInfo::Type::VehicleSet4:
        case CanDetectorInfo::Type::VehicleSet5:
        case CanDetectorInfo::Type::VehicleSet6:
        case CanDetectorInfo::Type::VehicleSet7:
        case CanDetectorInfo::Type::VehicleSet8:
        case CanDetectorInfo::Type::VehicleSet9:
        case CanDetectorInfo::Type::VehicleSet10:
        case CanDetectorInfo::Type::VehicleSet11:
        case CanDetectorInfo::Type::VehicleSet12:
        case CanDetectorInfo::Type::VehicleSet13:
        case CanDetectorInfo::Type::VehicleSet14:
        case CanDetectorInfo::Type::VehicleSet15:
            it->info.addVehicles(info.vehicles());
            it->info.addDirections(info.directions());
            continue;
        }

        qCWarning(core::logger<CanDetectorInfo>(), "Unsupported CAN info type: %d", core::value(info.type()));
    }

    auto result = QList<core::DetectorInfo>{};
    result.reserve(genericInfoList.size());

    std::transform(genericInfoList.constBegin(), genericInfoList.constEnd(),
                   std::back_inserter(result), [](const auto &chunk) {
        return chunk.info;
    });

    return result;
}

dcc::VehicleAddress CanDetectorInfo::vehicle(quint16 value) const
{
    if (isVehicleSet())
        return value & 0x3fff;

    return 0;
}

dcc::Direction CanDetectorInfo::direction(quint16 value) const
{
    if (isVehicleSet()) {
        switch (value & 0xc000) {
        case 0xc000:
            return dcc::Direction::Reverse;
        case 0x8000:
            return dcc::Direction::Forward;
        }
    }

    return dcc::Direction::Unknown;
}

bool TurnoutInfo::isValid() const
{
    return m_data.size() >= 3;
}

dcc::AccessoryAddress TurnoutInfo::address() const
{
    return (qFromBigEndian<quint16>(m_data.constData()) & 0x7ff) + 1;
}

dcc::TurnoutState TurnoutInfo::state() const
{
    return static_cast<dcc::TurnoutState>(m_data[2] & 3);
}

TurnoutInfo::TurnoutInfo(QByteArray data)
    : m_data{std::move(data)}
{}

RailcomInfo::RailcomInfo(QByteArray data)
    : m_data{std::move(data)}
{}

bool RailcomInfo::isValid() const
{
    return m_data.size() >= 2;
}

quint16 RailcomInfo::address() const
{
    return m_data.size() >= 2 ? qFromLittleEndian<quint16>(m_data.constData()) : 0;
}

quint32 RailcomInfo::receiveCounter() const
{
    return m_data.size() >= 6 ? qFromLittleEndian<quint32>(m_data.constData() + 2) : 0;
}

quint16 RailcomInfo::errorCounter() const
{
    return m_data.size() >= 8 ? qFromLittleEndian<quint16>(m_data.constData() + 6) : 0;
}

RailcomInfo::Options RailcomInfo::options() const
{
    return Options{m_data.size() >= 10 ? static_cast<quint8>(m_data[9]) : 0};
}

quint8 RailcomInfo::speed() const
{
    return m_data.size() >= 11 ? static_cast<quint8>(m_data[10]) : 0;
}

quint8 RailcomInfo::qos() const
{
    return m_data.size() >= 12 ? static_cast<quint8>(m_data[11]) : 0;
}

LibraryInfo::LibraryInfo(QByteArray data)
    : m_data{std::move(data)}
{}

quint16 LibraryInfo::address() const
{
    return qFromBigEndian<quint16>(m_data.constData());
}

quint8 LibraryInfo::index() const
{
    return static_cast<quint8>(m_data[2]);
}

quint8 LibraryInfo::flags() const
{
    return static_cast<quint8>(m_data[3]);
}

QString LibraryInfo::name() const
{
    return QString::fromLatin1(m_data.mid(4, 5).trimmed());
}

class Client::Private : public core::PrivateObject<Client>
{
public:
    explicit Private(Client *parent);

    // attributes
    auto isConnected() const { return m_socket != nullptr; }
    auto hostAddress() const { return m_hostAddress; }
    auto hostPort() const { return m_hostPort; }

    auto trackStatus() const { return m_deviceInfo.trackStatus; }
    void setTrackStatus(TrackStatus trackStatus);

    auto centralStatus() const { return m_deviceInfo.centralStatus; }
    void setCentralStatus(CentralStatus centralStatus);

    auto capabilities() const { return m_deviceInfo.capabilities; }
    void setCapabilities(Capabilities capabilities);

    auto subscriptions() const { return m_deviceInfo.subscriptions; }
    void setSubscriptions(Subscriptions subscriptions);

    auto serialNumber() const { return m_deviceInfo.serialNumber; }
    void setSerialNumber(quint32 serialNumber);

    auto firmwareVersion() const { return m_deviceInfo.firmwareVersion; }
    void setFirmwareVersion(QVersionNumber firmwareVersion);

    auto protocolVersion() const { return m_deviceInfo.protocolVersion; }
    void setProtocolVersion(QVersionNumber protocolVersion);

    auto centralId() const { return m_deviceInfo.centralId; }
    void setCentralId(int centralId);

    auto mainTrackCurrent() const { return m_deviceInfo.mainTrackCurrent; }
    void setMainTrackCurrent(core::milliamperes mainTrackCurrent);

    auto programmingTrackCurrent() const { return m_deviceInfo.programmingTrackCurrent; }
    void setProgrammingTrackCurrent(core::milliamperes programmingTrackCurrent);

    auto filteredMainTrackCurrent() const { return m_deviceInfo.filteredMainTrackCurrent; }
    void setFilteredMainTrackCurrent(core::milliamperes filteredMainTrackCurrent);

    auto temperature() const { return m_deviceInfo.temperature; }
    void setTemperature(core::celsius temperature);

    auto supplyVoltage() const { return m_deviceInfo.supplyVoltage; }
    void setSupplyVoltage(core::millivolts supplyVoltage);

    auto trackVoltage() const { return m_deviceInfo.trackVoltage; }
    void setTrackVoltage(core::millivolts trackVoltage);

    auto hardwareType() const { return m_deviceInfo.hardwareType; }
    void setHardwareType(HardwareType hardwareType);

    auto lockState() const { return m_deviceInfo.lockState; }
    void setLockState(LockState lockState);

    // operations
    void connectToHost(QHostAddress host, quint16 port);
    void disconnectFromHost();

    using Observer = std::function<bool(Message message)>;
    void sendRequest(QByteArray request, Observer observer);

    void startFeedbackModuleProgramming(rm::rbus::ModuleId module);
    void stopFeedbackModuleProgramming();
    void runFeedbackModuleProgramming();

    // parsers
    bool parseXStatusChanged(Message message);
    bool parseSystemStateDataChanged(Message message);

    std::optional<VehicleInfo> parseVehicleInfo(Message message);
    std::optional<AccessoryInfo> parseAccessoryInfo(Message message);
    std::optional<TurnoutInfo> parseTurnoutInfo(Message message);
    std::optional<RBusDetectorInfo> parseRBusDetectorInfo(Message message);
    std::optional<LoconetDetectorInfo> parseLoconetDetectorInfo(Message message);
    std::optional<CanDetectorInfo> parseCanDetectorInfo(Message message);
    std::optional<RailcomInfo> parseRailcomInfo(Message message);
    std::optional<LibraryInfo> parseLibraryInfo(Message message);

    std::variant<std::monostate, Error, ConfigurationVariable> parseProgrammingResponse(Message message);

    bool parseRequestEmergencyStopResponse(Message message);
    bool parseDisableTrackPowerResponse(Message message);
    bool parseEnableTrackPowerResponse(Message message);

    void startProgrammingTimeout(std::function<void()> callback);
    void stopProgrammingTimeout();
    void stopConnectTimeout();

    void reportError(Error error);

    using CanDetectorInfoCallback = std::function<void(QList<CanDetectorInfo>)>;
    void queueCanDetectorInfoCallback(rm::can::NetworkId networkId, CanDetectorInfoCallback callback);

    using LoconetDetectorInfoCallback = std::function<void(QList<LoconetDetectorInfo>)>;
    void queueLoconetDetectorInfoCallback(LoconetDetectorInfo::Query type, quint16 address,
                                          LoconetDetectorInfoCallback callback);

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void resetObservers();
    void parseBroadcasts(Message message);
    void parseDatagrams();

    void resendStarvedRequests();

    void emitSignalsOnIdle();
    void sendRequestsOnIdle();

    struct CanDetectorState
    {
        CanDetectorState() noexcept = default;

        CanDetectorInfo occupancy;
        QList<CanDetectorInfo> vehicleSets;

        bool isComplete() const noexcept;
        bool hasVehicles() const noexcept;

        const CanDetectorInfo &info() const noexcept;

        auto networkId() const noexcept { return info().networkId(); }
        auto module() const noexcept { return info().module(); }
        auto port() const noexcept { return info().port(); }
        auto key() const noexcept { return info().key(); }
    };

    struct CanNetworkState
    {
        QHash<CanDetectorInfo::Key, CanDetectorState> detectors;

        bool isComplete() const noexcept;
    };

    void mergeCanDetectorInfo(CanDetectorInfo info);
    void maybeEmitCanDetectorInfo(rm::can::NetworkId networkId, const CanNetworkState &networkState);

    void mergeLoconetDetectorInfo(LoconetDetectorInfo info);
    void emitPendingLoconetDetectorInfo();

    // attributes
    QHostAddress m_hostAddress;
    quint16 m_hostPort = 0;

    struct DeviceInfo
    {
        TrackStatus trackStatus = TrackStatus::PowerOff; // FIXME: reset all these attributes while connecting?
        CentralStatus centralStatus;
        Capabilities capabilities;
        Subscriptions subscriptions;
        quint32 serialNumber = 0;
        QVersionNumber firmwareVersion;
        QVersionNumber protocolVersion;
        int centralId = 0;
        core::milliamperes mainTrackCurrent;
        core::milliamperes programmingTrackCurrent;
        core::milliamperes filteredMainTrackCurrent;
        core::celsius temperature;
        core::millivolts supplyVoltage;
        core::millivolts trackVoltage;
        HardwareType hardwareType = HardwareType::Unknown;
        LockState lockState = LockState::Invalid;
    };

    DeviceInfo m_deviceInfo;

    // internal state
    struct PendingRequest
    {
        using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;

        PendingRequest() = default;
        PendingRequest(QByteArray data, Observer observer, Timestamp timestamp = Timestamp::clock::now()) noexcept
            : data{std::move(data)}
            , observe{std::move(observer)}
            , timestamp{std::move(timestamp)}
        {}

        QByteArray data;
        Observer observe;
        Timestamp timestamp = Timestamp::clock::now();
    };

    QList<PendingRequest> m_pendingRequests;

    QUdpSocket *m_socket = nullptr;
    QList<QByteArray> m_sendBuffer;
    QByteArray m_receiveBuffer;

    Timer m_programmingTimeout{this};
    std::function<void()> m_programmingCallback;

    Timer m_connectTimeout{this};
    Timer m_resendTimer{this};
    Timer m_idleTimer{this};

    Timer m_feedbackProgrammingTimer{this};
    QByteArray m_feedbackProgrammingRequest;

    QHash<rm::can::NetworkId, CanNetworkState> m_canDetectorStates;
    QHash<rm::can::NetworkId, QList<CanDetectorInfoCallback>> m_canDetectorCallbacks;

    QHash<quint32, QList<LoconetDetectorInfo>> m_pendingLoconetDetectorInfo;
    QHash<quint32, QList<LoconetDetectorInfoCallback>> m_loconetDetectorInfoCallbacks;
};

int Message::length() const
{
    return !m_data.isEmpty() ? qFromLittleEndian<quint16>(m_data.constData()) : 0;
}

LanMessageId Message::lanMessageId() const
{
    return qFromLittleEndian<LanMessageId>(m_data.constData() + 2);
}

XBusMessageId Message::xbusMessageId() const
{
    if (length() >= 5 && lanMessageId() == LanMessageId::XNetMessage) {
        static const auto metaEnum = QMetaEnum::fromType<XBusMessageId>();

        if (const auto shortId = static_cast<quint8>(m_data[4]); metaEnum.valueToKey(shortId))
            return static_cast<XBusMessageId>(shortId);
        if (length() >= 6)
            return static_cast<XBusMessageId>(qFromBigEndian<quint16>(m_data.constData() + 4));
    }

    return XBusMessageId::Invalid;
}

Client::Private::Private(Client *parent)
    : PrivateObject{parent}
{
    resetObservers();
}

void Client::Private::setTrackStatus(TrackStatus trackStatus)
{
    if (std::exchange(m_deviceInfo.trackStatus, trackStatus) != trackStatus)
        emit q()->trackStatusChanged(m_deviceInfo.trackStatus);
}

void Client::Private::setCentralStatus(CentralStatus centralStatus)
{
    if (std::exchange(m_deviceInfo.centralStatus, centralStatus) != centralStatus)
        emit q()->centralStatusChanged(m_deviceInfo.centralStatus);
}

void Client::Private::setCapabilities(Capabilities capabilities)
{
    if (std::exchange(m_deviceInfo.capabilities, capabilities) != capabilities)
        emit q()->capabilitiesChanged(m_deviceInfo.capabilities);
}

void Client::Private::setSubscriptions(Subscriptions subscriptions)
{
    if (std::exchange(m_deviceInfo.subscriptions, subscriptions) != subscriptions)
        emit q()->subscriptionsChanged(m_deviceInfo.subscriptions);
}

void Client::Private::setSerialNumber(quint32 serialNumber)
{
    if (std::exchange(m_deviceInfo.serialNumber, serialNumber) != serialNumber)
        emit q()->serialNumberChanged(m_deviceInfo.serialNumber);
}

void Client::Private::setFirmwareVersion(QVersionNumber firmwareVersion)
{
    if (std::exchange(m_deviceInfo.firmwareVersion, firmwareVersion) != firmwareVersion)
        emit q()->firmwareVersionChanged(m_deviceInfo.firmwareVersion);
}

void Client::Private::setProtocolVersion(QVersionNumber protocolVersion)
{
    if (std::exchange(m_deviceInfo.protocolVersion, protocolVersion) != protocolVersion)
        emit q()->protocolVersionChanged(m_deviceInfo.protocolVersion);
}

void Client::Private::setCentralId(int centralId)
{
    if (std::exchange(m_deviceInfo.centralId, centralId) != centralId)
        emit q()->centralIdChanged(m_deviceInfo.centralId);
}

void Client::Private::setMainTrackCurrent(core::milliamperes mainTrackCurrent)
{
    if (std::exchange(m_deviceInfo.mainTrackCurrent, mainTrackCurrent) != mainTrackCurrent)
        emit q()->mainTrackCurrentChanged(m_deviceInfo.mainTrackCurrent);
}

void Client::Private::setProgrammingTrackCurrent(core::milliamperes programmingTrackCurrent)
{
    if (std::exchange(m_deviceInfo.programmingTrackCurrent, programmingTrackCurrent) != programmingTrackCurrent)
        emit q()->programmingTrackCurrentChanged(m_deviceInfo.programmingTrackCurrent);
}

void Client::Private::setFilteredMainTrackCurrent(core::milliamperes filteredMainTrackCurrent)
{
    if (std::exchange(m_deviceInfo.filteredMainTrackCurrent, filteredMainTrackCurrent) != filteredMainTrackCurrent)
        emit q()->filteredMainTrackCurrentChanged(m_deviceInfo.filteredMainTrackCurrent);
}

void Client::Private::setTemperature(core::celsius temperature)
{
    if (std::exchange(m_deviceInfo.temperature, temperature) != temperature)
        emit q()->temperatureChanged(m_deviceInfo.temperature);
}

void Client::Private::setSupplyVoltage(core::millivolts supplyVoltage)
{
    if (std::exchange(m_deviceInfo.supplyVoltage, supplyVoltage) != supplyVoltage)
        emit q()->supplyVoltageChanged(m_deviceInfo.supplyVoltage);
}

void Client::Private::setTrackVoltage(core::millivolts trackVoltage)
{
    if (std::exchange(m_deviceInfo.trackVoltage, trackVoltage) != trackVoltage)
        emit q()->trackVoltageChanged(m_deviceInfo.trackVoltage);
}

void Client::Private::setHardwareType(HardwareType hardwareType)
{
    if (std::exchange(m_deviceInfo.hardwareType, hardwareType) != hardwareType)
        emit q()->hardwareTypeChanged(m_deviceInfo.hardwareType);
}

void Client::Private::setLockState(LockState lockState)
{
    if (std::exchange(m_deviceInfo.lockState, lockState) != lockState)
        emit q()->lockStateChanged(m_deviceInfo.lockState);
}

void Client::Private::connectToHost(QHostAddress host, quint16 port)
{
    const auto isConnectedGuard = core::propertyGuard(q(), &Client::isConnected, &Client::isConnectedChanged);
    const auto hostAddressGuard = core::propertyGuard(q(), &Client::hostAddress, &Client::hostAddressChanged);
    const auto hostPortGuard = core::propertyGuard(q(), &Client::hostPort, &Client::hostPortChanged);

    disconnectFromHost();

    m_socket = new QUdpSocket{this};
    m_connectTimeout.start(2s);
    m_resendTimer.start(1s);
    m_idleTimer.start(50ms);
    m_hostAddress = std::move(host);
    m_hostPort = port;

    connect(m_socket, &QUdpSocket::readyRead, this, &Private::parseDatagrams);
}

void Client::Private::disconnectFromHost()
{
    const auto isConnectedGuard = core::propertyGuard(q(), &Client::isConnected, &Client::isConnectedChanged);

    if (const auto socket = std::exchange(m_socket, nullptr)) {
        socket->disconnect(q());
        socket->deleteLater();
    }

    stopProgrammingTimeout();
    m_connectTimeout.stop();
    m_resendTimer.stop();

    m_receiveBuffer.clear();
    m_sendBuffer.clear();
    resetObservers();

    if (isConnectedGuard.hasChanged())
        emit q()->disconnected();
}

void Client::Private::sendRequest(QByteArray request, Observer observer)
{
    if (m_feedbackProgrammingTimer.isActive()
            && request != m_feedbackProgrammingRequest)
        stopFeedbackModuleProgramming();

    if (observer)
        m_pendingRequests.emplaceBack(request, std::move(observer));

    qCDebug(lcStream, "queuing %s", request.toHex(' ').constData());
    m_sendBuffer.append(std::move(request));
}

void Client::Private::startFeedbackModuleProgramming(rm::rbus::ModuleId module)
{
    if (m_feedbackProgrammingTimer.start(1s)) {
        m_feedbackProgrammingRequest = "05 00 82 00 00"_hex;
        qToBigEndian(static_cast<quint8>(module), m_feedbackProgrammingRequest.data() + 4);
        runFeedbackModuleProgramming();
    }
}

void Client::Private::stopFeedbackModuleProgramming()
{
    if (m_feedbackProgrammingTimer.stop())
        sendRequest("05 00 82 00 00"_hex, {});
}

void Client::Private::runFeedbackModuleProgramming()
{
    if (m_feedbackProgrammingTimer.isActive())
        sendRequest(m_feedbackProgrammingRequest, {});
}

bool Client::Private::parseXStatusChanged(Message message)
{
    if (message.xbusMessageId() == XBusMessageId::StatusChanged && message.length() >= 8) {
        setTrackStatus(static_cast<TrackStatus>(message.xbusData()[1]));
        return true;
    }

    return false;
}

bool Client::Private::parseSystemStateDataChanged(Message message)
{
    if (message.lanMessageId() == LanMessageId::SystemStateDataChanged && message.length() >= 20) {
        const auto data = message.lanData();
        setMainTrackCurrent(qFromLittleEndian<qint16>(data));
        setProgrammingTrackCurrent(qFromLittleEndian<qint16>(data + 2));
        setFilteredMainTrackCurrent(qFromLittleEndian<qint16>(data + 4));
        setTemperature(qFromLittleEndian<qint16>(data + 6));
        setSupplyVoltage(qFromLittleEndian<quint16>(data + 8));
        setTrackVoltage(qFromLittleEndian<quint16>(data + 10));
        setCentralStatus(static_cast<CentralStatus>(qFromLittleEndian<quint16>(data + 12)));
        setCapabilities(static_cast<Capabilities>(qFromLittleEndian<quint8>(data + 15)));
        return true;
    }

    return false;
}

std::optional<VehicleInfo> Client::Private::parseVehicleInfo(Message message)
{
    if (message.xbusMessageId() == XBusMessageId::VehicleInfo && message.length() >= 11)
        return VehicleInfo{message.rawData().mid(5)};

    return {};
}

std::optional<AccessoryInfo> Client::Private::parseAccessoryInfo(Message message)
{
    if (message.xbusMessageId() == XBusMessageId::AccessoryInfo && message.length() >= 10)
        return AccessoryInfo{message.rawData().mid(5)};

    return {};
}

std::optional<TurnoutInfo> Client::Private::parseTurnoutInfo(Message message)
{
    if (message.xbusMessageId() == XBusMessageId::TurnoutInfo && message.length() >= 9)
        return TurnoutInfo{message.rawData().mid(5)};

    return {};
}

std::optional<RBusDetectorInfo> Client::Private::parseRBusDetectorInfo(Message message)
{
    if (message.lanMessageId() == LanMessageId::RBusDetectorDataChanged && message.length() >= 11)
        return RBusDetectorInfo{message.rawData().mid(4)};

    return {};
}

std::optional<LoconetDetectorInfo> Client::Private::parseLoconetDetectorInfo(Message message)
{
    if (message.lanMessageId() == LanMessageId::LoconetDetectorDataChanged && message.length() >= 7)
        return LoconetDetectorInfo{message.rawData().mid(4)};

    return {};
}

std::optional<CanDetectorInfo> Client::Private::parseCanDetectorInfo(Message message)
{
    if (message.lanMessageId() == LanMessageId::CanDetectorDataChanged && message.length() >= 7)
        return CanDetectorInfo{message.rawData().mid(4)};

    return {};
}

std::optional<RailcomInfo> Client::Private::parseRailcomInfo(Message message)
{
    if (message.lanMessageId() == LanMessageId::RailcomDataChanged) {
        if (message.length() >= 11)
            return RailcomInfo{message.rawData().mid(4)};

        return RailcomInfo{};
    }

    return {};
}

std::optional<LibraryInfo> Client::Private::parseLibraryInfo(Message message)
{
    if (message.xbusMessageId() == XBusMessageId::LibraryInfo && message.length() >= 16)
        return LibraryInfo{message.rawData().mid(6)};

    return {};
}

std::variant<std::monostate, Client::Error, ConfigurationVariable>
Client::Private::parseProgrammingResponse(Message message)
{
    if (message.xbusMessageId() == XBusMessageId::ConfigErrorShortCircuit)
        return ShortCircuitError;
    if (message.xbusMessageId() == XBusMessageId::ConfigErrorValueRejected)
        return ValueRejectedError;
    if (message.xbusMessageId() == XBusMessageId::ConfigResult && message.length() >= 10) {
        const auto cv = static_cast<quint16>(qFromBigEndian<quint16>(message.xbusData() + 1) + 1);
        const auto value = message.xbusData()[3];
        return ConfigurationVariable{cv, value};
    }

    return {};
}

bool Client::Private::parseRequestEmergencyStopResponse(Message message)
{
    if (message.xbusMessageId() == XBusMessageId::BroadcastEmergencyStop) {
        setTrackStatus(TrackStatus::EmergencyStop);
        return true;
    }

    return false;
}

bool Client::Private::parseDisableTrackPowerResponse(Message message)
{
    if (message.xbusMessageId() == XBusMessageId::BroadcastPowerOff) {
        setTrackStatus(TrackStatus::PowerOff);
        return true;
    }

    return false;
}

bool Client::Private::parseEnableTrackPowerResponse(Message message)
{
    if (message.xbusMessageId() == XBusMessageId::BroadcastPowerOn) {
        setTrackStatus(TrackStatus::PowerOn);
        return true;
    }

    return false;
}

void Client::Private::startProgrammingTimeout(std::function<void()> callback)
{
    stopProgrammingTimeout();

    if (m_programmingTimeout.start(5s))
        m_programmingCallback = std::move(callback);
}

void Client::Private::stopProgrammingTimeout()
{
    m_programmingCallback = {};
    m_programmingTimeout.stop();
}

void Client::Private::stopConnectTimeout()
{
    m_connectTimeout.stop();
}

void Client::Private::reportError(Error error)
{
    qCWarning(logger()) << "An error occured:" << error;
    emit q()->errorOccured(error);
}

void Client::Private::queueCanDetectorInfoCallback(rm::can::NetworkId networkId, CanDetectorInfoCallback callback)
{
    if (callback)
        m_canDetectorCallbacks[networkId] += std::move(callback);
}

void Client::Private::queueLoconetDetectorInfoCallback(LoconetDetectorInfo::Query type, quint16 address,
                                                       LoconetDetectorInfoCallback callback)
{
    if (callback)
        m_loconetDetectorInfoCallbacks[makeKey(type, address)] += std::move(callback);
}

void Client::Private::timerEvent(QTimerEvent *event)
{
    if (m_programmingTimeout.matches(event)) {
        const auto callback = m_programmingCallback;
        stopProgrammingTimeout();
        callback();
    } else if (m_connectTimeout.matches(event)) {
        qCWarning(logger(), "Connect timeout reached. Aborting.");
        disconnectFromHost();
    } else if (m_resendTimer.matches(event)) {
        resendStarvedRequests();
    } else if (m_idleTimer.matches(event)) {
        emitSignalsOnIdle();
        sendRequestsOnIdle();
    } else if (m_feedbackProgrammingTimer.matches(event)) {
        runFeedbackModuleProgramming();
    }
}

void Client::Private::resetObservers()
{
    m_pendingRequests = {
        {
            {}, [this](auto message) {
                parseBroadcasts(std::move(message));
                return false;
            }
        }
    };
}

void Client::Private::parseDatagrams()
{
    char buffer[4096];
    QHostAddress host;
    quint16 port;

    while (m_socket->hasPendingDatagrams()) {
        if (const auto bytesReceived = m_socket->readDatagram(buffer, sizeof buffer, &host, &port); bytesReceived > 0) {
            if (port != m_hostPort) {
                qCWarning(lcStream, "Ignoring datagram from unknown port: %d", port);
                continue;
            }

            if (m_hostAddress.protocol() == QUdpSocket::IPv4Protocol) {
                if (m_hostAddress.toIPv4Address() != host.toIPv4Address()) {
                    qCWarning(lcStream, "Ignoring datagram from unknown host: %ls", qUtf16Printable(host.toString()));
                    continue;
                }
            } else if (m_hostAddress.protocol() == QUdpSocket::IPv6Protocol) {
                if (memcmp(m_hostAddress.toIPv6Address().c, host.toIPv6Address().c, 8) != 0) {
                    qCWarning(lcStream, "Ignoring datagram from unknown host: %ls", qUtf16Printable(host.toString()));
                    continue;
                }
            } else {
                break;
            }

            m_receiveBuffer.append(buffer, bytesReceived);

            while (!m_receiveBuffer.isEmpty()) {
                if (const auto messageLen = m_receiveBuffer.at(0); messageLen <= m_receiveBuffer.size()) {
                    const auto message = Message{m_receiveBuffer.left(messageLen)};
                    m_receiveBuffer.remove(0, messageLen);

                    qCDebug(lcStream, "received %s", message.rawData().toHex(' ').constData());

                    // move observers aside in case one of the callbacks adds more observers, invalidating the list
                    decltype(m_pendingRequests) pendingRequests;
                    std::swap(pendingRequests, m_pendingRequests);

                    for (auto it = pendingRequests.begin(); it != pendingRequests.end(); ) {
                        if (it->observe(message))
                            it = pendingRequests.erase(it);
                        else
                            ++it;
                    }

                    // restore observers
                    std::swap(pendingRequests, m_pendingRequests);

                    // add new observers added from callbacks while processing the current list
                    if (!pendingRequests.empty()) {
                        qCDebug(lcStream, "Adding %zd observers from callbacks", pendingRequests.size());
                        m_pendingRequests.reserve(m_pendingRequests.size() + pendingRequests.size());
                        std::copy(pendingRequests.begin(), pendingRequests.end(), std::back_inserter(m_pendingRequests));
                    }
                } else {
                    qCDebug(lcStream, "ignoring incomplete data %s", m_receiveBuffer.toHex(' ').constData());
                    break;
                }
            }
        }
    }
}

void Client::Private::resendStarvedRequests()
{
    const auto now = PendingRequest::Timestamp::clock::now();
    auto first = true;

    for (auto &request: m_pendingRequests) {
        if (request.data.isEmpty())
            continue;

        if (const auto age = now - request.timestamp; age >= 2s) {
            if (std::exchange(first, false))
                qInfo() << "starved requests:";

            qInfo() << std::chrono::duration_cast<std::chrono::milliseconds>(age)
                    << request.data.toHex(' ') << "resending";

            sendRequest(request.data, {});
            request.timestamp = now;
        }
    }
}

void Client::Private::sendRequestsOnIdle()
{
    if (m_socket && !m_sendBuffer.isEmpty()) {
        constexpr auto MaximumDatagramSize = 1472;

        auto requestCount = 0;
        auto datagram = QByteArray{};

        while (!m_sendBuffer.isEmpty()
               && (datagram.size() + m_sendBuffer.first().size()) <= MaximumDatagramSize) {
            datagram.append(m_sendBuffer.takeFirst());
            ++requestCount;
        }

        if (!datagram.isEmpty()) {
            qCDebug(lcStream, "sending %d request(s) in a datagram of %zd bytes", requestCount, datagram.size());
            qCDebug(lcStream) << datagram.toHex(' ');
            m_socket->writeDatagram(std::move(datagram), m_hostAddress, m_hostPort);
        }
    }
}

void Client::Private::emitSignalsOnIdle()
{
    emitPendingLoconetDetectorInfo();
}

void Client::Private::mergeCanDetectorInfo(CanDetectorInfo info)
{
    qInfo() << Q_FUNC_INFO << info << Qt::hex << info.value1() << info.value2() << info.data().toHex(' ');

    const auto networkId = info.networkId();
    auto networkState = m_canDetectorStates.find(networkId);

    if (networkState == m_canDetectorStates.end()) {
        qCInfo(lcCanDetector, "New CAN detector network found (network id: 0x%04x)", networkId.value);
        networkState = m_canDetectorStates.insert(networkId, {});
    }

    auto detectorState = networkState->detectors.find(info.key());

    if (detectorState == networkState->detectors.end()) {
        qCInfo(lcCanDetector, "New CAN detector found (network id: 0x%04x, module: %d, port: %d)",
               networkId.value, info.module().value, info.port().value);
        detectorState = networkState->detectors.insert(info.key(), {});
    }

    switch (info.type()) {
    case CanDetectorInfo::Type::Occupancy:
        detectorState->occupancy = std::move(info);
        break;

    case CanDetectorInfo::Type::VehicleSet1:
        detectorState->vehicleSets = {std::move(info)};
        break;

    case CanDetectorInfo::Type::VehicleSet2:
    case CanDetectorInfo::Type::VehicleSet3:
    case CanDetectorInfo::Type::VehicleSet4:
    case CanDetectorInfo::Type::VehicleSet5:
    case CanDetectorInfo::Type::VehicleSet6:
    case CanDetectorInfo::Type::VehicleSet7:
    case CanDetectorInfo::Type::VehicleSet8:
    case CanDetectorInfo::Type::VehicleSet9:
    case CanDetectorInfo::Type::VehicleSet10:
    case CanDetectorInfo::Type::VehicleSet11:
    case CanDetectorInfo::Type::VehicleSet12:
    case CanDetectorInfo::Type::VehicleSet13:
    case CanDetectorInfo::Type::VehicleSet14:
    case CanDetectorInfo::Type::VehicleSet15:
        detectorState->vehicleSets += std::move(info);
        break;
    }

    maybeEmitCanDetectorInfo(networkId, *networkState);
}

bool Client::Private::CanDetectorState::isComplete() const noexcept
{
    if (!occupancy.isValid()
            || vehicleSets.isEmpty()
            || !vehicleSets.last().isLastVehicleSet())
        return false;

    return hasVehicles() == (occupancy.occupancy() == CanDetectorInfo::Occupancy::Occupied);
}

bool Client::Private::CanDetectorState::hasVehicles() const noexcept
{
    if (vehicleSets.isEmpty())
        return false;

    const auto &last = vehicleSets.last();
    return last.type() != CanDetectorInfo::Type::VehicleSet1
            || last.vehicle1() != 0;
}

const CanDetectorInfo &Client::Private::CanDetectorState::info() const noexcept
{
    if (occupancy.isValid())
        return occupancy;
    if (!vehicleSets.isEmpty())
        return vehicleSets.first();

    static const auto invalid = CanDetectorInfo{};
    return invalid;
}

bool Client::Private::CanNetworkState::isComplete() const noexcept
{
    for (const auto &state: detectors) {
        if (!state.isComplete())
            return false;
    }

    return true;
}

void Client::Private::maybeEmitCanDetectorInfo(rm::can::NetworkId networkId, const CanNetworkState &networkState)
{
    //    Q_ASSERT(!infoList.isEmpty()); // FIXME: soft assert

    //    const auto isOccupancy = [](const CanDetectorInfo &info) {
//        return info.type() == CanDetectorInfo::Type::Occupancy;
//    };

//    const auto isLastVehicleInfo = [](const CanDetectorInfo &info) {
//        qCInfo(lcCanDetector) << "isLastVehicleInfo" << info.type() << info.vehicle2();
//        return CanDetectorInfo::VehicleSetRange.contains(info.type()) && info.vehicle2() == 0;
//    };

//    const auto begin = infoList.constBegin();
//    const auto end = infoList.constEnd();
//    const auto networkId = begin->networkId();
//    const auto baseInfo = std::find_if(begin, end, isOccupancy);
//    const auto lastVehicleSet = std::find_if(begin, end, isLastVehicleInfo);

    if (!networkState.isComplete())
        return;

    const auto networkAndWildcardCallbacks = std::exchange(m_canDetectorCallbacks[networkId], {})
            + std::exchange(m_canDetectorCallbacks[rm::can::NetworkIdAny], {});

    for (const auto &state: networkState.detectors) {
        qCInfo(lcCanDetector, "Emitting CAN detector info (network id: 0x%04x, module: %d, port: %d)",
               state.networkId().value, state.module().value, state.port().value);

        auto infoList = QList{state.occupancy} + state.vehicleSets;
        auto genericInfo = CanDetectorInfo::merge(infoList);

        for (const auto &verifiedInfo: infoList)
            Q_ASSERT(verifiedInfo.key() == state.key());

        for (const auto &callback: networkAndWildcardCallbacks)
            callback(infoList);

        qCInfo(lcCanDetector).verbosity(QDebug::MinimumVerbosity)
                << Qt::endl << "FOO:CAN:" << infoList
                << Qt::endl << "FOO:generic:" << genericInfo;

        emit q()->canDetectorInfoReceived(std::move(infoList));
        emit q()->detectorInfoReceived(std::move(genericInfo));
    }
}

void Client::Private::mergeLoconetDetectorInfo(LoconetDetectorInfo info)
{
    qInfo() << Q_FUNC_INFO << info << info.data().toHex(' ');
    Q_UNIMPLEMENTED();
}

void Client::Private::emitPendingLoconetDetectorInfo()
{
}

void Client::Private::parseBroadcasts(Message message)
{
    if (const auto info = parseVehicleInfo(message)) {
        emit q()->vehicleInfoReceived(info.value());
        return;
    }

    switch (message.length()) {
    case 7:
        if (const auto xbusId = message.xbusMessageId();
                xbusId == XBusMessageId::BroadcastPowerOff)
            setTrackStatus(TrackStatus::PowerOff);
        else if (xbusId == XBusMessageId::BroadcastPowerOn)
            setTrackStatus(TrackStatus::PowerOn);
        else if (xbusId == XBusMessageId::BroadcastProgrammingMode)
            setTrackStatus(TrackStatus::ProgrammingMode);
        else if (xbusId == XBusMessageId::BroadcastShortCircuit)
            setTrackStatus(TrackStatus::ShortCircuit);
        else if (xbusId == XBusMessageId::ConfigErrorShortCircuit)
            reportError(ShortCircuitError);
        else if (xbusId == XBusMessageId::ConfigErrorValueRejected)
            reportError(ValueRejectedError);
        else if (xbusId == XBusMessageId::UnknownCommand)
            reportError(UnknownCommandError);
        else if (xbusId == XBusMessageId::BroadcastEmergencyStop)
            setTrackStatus(TrackStatus::EmergencyStop);

        break;

    case 8:
        if (const auto info = parseLoconetDetectorInfo(message))
            mergeLoconetDetectorInfo(info.value());
        else
            parseXStatusChanged(std::move(message));

        break;

    case 9:
        if (const auto info = parseLoconetDetectorInfo(message))
            mergeLoconetDetectorInfo(info.value());
        else if (const auto info = parseTurnoutInfo(std::move(message)))
            emit q()->turnoutInfoReceived(info.value());

        break;

    case 10:
        if (const auto info = parseLoconetDetectorInfo(message))
            mergeLoconetDetectorInfo(info.value());
        else if (const auto info = parseAccessoryInfo(std::move(message)))
            emit q()->accessoryInfoReceived(info.value());

        break;

    case 11:
    case 17:
        if (const auto info = parseRailcomInfo(std::move(message)))
            emit q()->railcomInfoReceived(info.value());

        break;

    case 14:
        if (const auto info = parseCanDetectorInfo(std::move(message)))
            mergeCanDetectorInfo(info.value());

        break;

    case 15:
        if (const auto info = parseRBusDetectorInfo(std::move(message)))
            emit q()->rbusDetectorInfoReceived(info.value());

        break;

    case 16:
        if (const auto info = parseLibraryInfo(std::move(message)))
            emit q()->libraryInfoReceived(info.value());

        break;

    case 20:
        parseSystemStateDataChanged(std::move(message));
        break;
    }
}

// lifetime //

Client::Client(QObject *parent)
    : QObject{parent}
    , d{new Private{this}}
{
    qRegisterMetaType<Client>();
    qRegisterMetaType<Client::Capability>();
    qRegisterMetaType<Client::Capabilities>();
    qRegisterMetaType<Client::CentralStatus>();
    qRegisterMetaType<Client::CentralStatusFlag>();

    connect(this, &Client::rbusDetectorInfoReceived, this, &Client::detectorInfoReceived);
}

Client::~Client()
{
    delete d;
}

// attributes //

Client::TrackStatus Client::trackStatus() const
{
    return d->trackStatus();
}

Client::CentralStatus Client::centralStatus() const
{
    return d->centralStatus();
}

Client::Capabilities Client::capabilities() const
{
    return d->capabilities();
}

Client::Subscriptions Client::subscriptions() const
{
    return d->subscriptions();
}

quint32 Client::serialNumber() const
{
    return d->serialNumber();
}

QVersionNumber Client::firmwareVersion() const
{
    return d->firmwareVersion();
}

QVersionNumber Client::protocolVersion() const
{
    return d->protocolVersion();
}

int Client::centralId() const
{
    return d->centralId();
}

QHostAddress Client::hostAddress() const
{
    return d->hostAddress();
}

quint16 Client::hostPort() const
{
    return d->hostPort();
}

bool Client::isConnected() const
{
    return d->isConnected();
}

core::milliamperes Client::mainTrackCurrent() const
{
    return d->mainTrackCurrent();
}

core::milliamperes Client::programmingTrackCurrent() const
{
    return d->programmingTrackCurrent();
}

core::milliamperes Client::filteredMainTrackCurrent() const
{
    return d->filteredMainTrackCurrent();
}

core::celsius Client::temperature() const
{
    return d->temperature();
}

core::millivolts Client::supplyVoltage() const
{
    return d->supplyVoltage();
}

core::millivolts Client::trackVoltage() const
{
    return d->trackVoltage();
}

Client::HardwareType Client::hardwareType() const
{
    return d->hardwareType();
}

Client::LockState Client::lockState() const
{
    return d->lockState();
}

QString Client::hardwareName(HardwareType type)
{
    switch (type) {
    case HardwareType::Z21Old:
        return tr("Z21 (black, since 2012)");
    case HardwareType::Z21New:
        return tr("Z21 (black, since 2013)");
    case HardwareType::SmartRail:
        return tr("SmartRail (since 2012)");
    case HardwareType::Z21Small:
        return tr("Z21 (since 2013, from starter set)");
    case HardwareType::Z21Start:
        return tr("z21 Start (since 2016, from starter set)");
    case HardwareType::SingleBooster:
        return tr("Z21 Single Booster (zLink)");
    case HardwareType::DualBooster:
        return tr("Z21 Dual Booster (zLink)");
    case HardwareType::Z21XL:
        return tr("Z21 XL Series (2020)");
    case HardwareType::XLBooster:
        return tr("Z21 XL Booster (2021, zLink)");
    case HardwareType::Z21SwitchDecoder:
        return tr("Z21 SwitchDecoder (zLink)");
    case HardwareType::Z21SignalDecoder:
        return tr("Z21 SignalDecoder (zLink)");
    case HardwareType::Unknown:
        break;
    }

    return {};
}

// operations //

void Client::disableTrackPower(std::function<void(TrackStatus state)> callback)
{
    d->sendRequest("07 00 40 00 21 80 a1"_hex, [this, callback](auto message) {
        if (d->parseDisableTrackPowerResponse(std::move(message))) {
            callIfDefined(callback, trackStatus());
            return true;
        }

        return false;
    });
}

void Client::enableTrackPower(std::function<void(TrackStatus state)> callback)
{
    d->sendRequest("07 00 40 00 21 81 a0"_hex, [this, callback](auto message) {
        if (d->parseEnableTrackPowerResponse(std::move(message))) {
            callIfDefined(callback, trackStatus());
            return true;
        }

        return false;
    });
}

void Client::requestEmergencyStop(std::function<void (TrackStatus)> callback)
{
    d->sendRequest("06 00 40 00 80 80"_hex, [this, callback](auto message) {
        if (d->parseRequestEmergencyStopResponse(std::move(message))) {
            callIfDefined(callback, trackStatus());
            return true;
        }

        return false;
    });
}

void Client::subscribe(Subscriptions subscriptions)
{
    auto request = "08 00 50 00 00 00 00 00"_hex;
    qToLittleEndian(static_cast<quint32>(subscriptions), request.data() + 4);
    d->sendRequest(request, {});
}

void Client::logoff()
{
    d->sendRequest("04 00 30 00"_hex, {});
}

void Client::setSpeed14(dcc::VehicleAddress address, dcc::Speed14 speed, dcc::Direction direction)
{
    auto request = "0A 00 40 00 E4 11 00 00 00 00"_hex;
    qToBigEndian<quint16>(address & 0x3fff, request.data() + 6);
    qToBigEndian<quint8>((direction == dcc::Direction::Forward ? 128_u8 : 0_u8)
                         | ((speed.count() / 2) & 15),
                         request.data() + 8);
    updateChecksum(&request);

    d->sendRequest(request, {});
}

void Client::setSpeed28(dcc::VehicleAddress address, dcc::Speed28 speed, dcc::Direction direction)
{
    auto request = "0A 00 40 00 E4 12 00 00 00 00"_hex;
    qToBigEndian<quint16>(address & 0x3fff, request.data() + 6);
    qToBigEndian<quint8>((direction == dcc::Direction::Forward ? 128_u8 : 0_u8)
                         | (speed.count() & 1 ? 16_u8 : 0_u8)
                         | ((speed.count() / 2) & 15),
                         request.data() + 8);
    updateChecksum(&request);
    d->sendRequest(request, {});
}

void Client::setSpeed126(dcc::VehicleAddress address, dcc::Speed126 speed, dcc::Direction direction)
{
    auto request = "0A 00 40 00 E4 13 00 00 00 00"_hex;
    qToBigEndian<quint16>(address & 0x3fff, request.data() + 6);
    qToBigEndian<quint8>((direction == dcc::Direction::Forward ? 128_u8 : 0_u8)
                         | (speed.count() & 127),
                         request.data() + 8);
    updateChecksum(&request);
    d->sendRequest(request, {});
}

void Client::setFunction(dcc::VehicleAddress address, dcc::Function function, SetFunctionMode mode)
{
    auto request = "0A 00 40 00 E4 F8 00 00 00 00"_hex;
    qToBigEndian<quint16>(address & 0x3fff, request.data() + 6);
    qToBigEndian<quint8>(static_cast<quint8>(mode << 6)
                         | (function & 63), request.data() + 8);

    updateChecksum(&request);
    d->sendRequest(request, {});
}

void Client::setAccessoryState(dcc::AccessoryAddress address, quint8 state)
{
    auto request = "0A 00 40 00 54 00 00 00 00 00"_hex;
    qToBigEndian<quint16>((address - 1) & 0x07ff, request.data() + 5);
    qToBigEndian<quint8>(state, request.data() + 7);

    updateChecksum(&request);
    d->sendRequest(request, {});
}

void Client::setTurnoutState(dcc::AccessoryAddress address, TurnoutDirection direction, bool enabled)
{
    const auto directionFlag = static_cast<quint8>(direction == Straight ? 1 : 0);
    const auto enableFlag = static_cast<quint8>(enabled ? 8 : 0);

    auto request = "09 00 40 00 53 00 00 00 00 00"_hex;
    qToBigEndian<quint16>((address - 1) & 0x07ff, request.data() + 5);
    qToBigEndian<quint8>(0xA0 | directionFlag | enableFlag, request.data() + 7);

    updateChecksum(&request);
    d->sendRequest(request, {});
}

void Client::setTurnoutState(dcc::AccessoryAddress address, TurnoutDirection direction, std::chrono::milliseconds duration)
{
    const auto dt = static_cast<quint8>(qBound(0, duration.count() / 100, 127));
    const auto directionFlag = static_cast<quint8>(direction == Straight ? 128 : 0);
    setAccessoryState(address, directionFlag | dt);
}

void Client::requestAccessoryStop()
{
    auto request = "0A 00 40 00 54 07 FF 00 00 00"_hex;
    updateChecksum(&request);
    d->sendRequest(request, {});
}

// queries //

void Client::queryTrackStatus(std::function<void(TrackStatus)> callback)
{
    d->sendRequest("07 00 40 00 21 24 00"_hex, [this, callback](auto message) {
        if (d->parseXStatusChanged(std::move(message))) {
            callIfDefined(callback, d->trackStatus());
            return true;
        }

        return false;
    });
}

void Client::querySubscriptions(std::function<void(Subscriptions)> callback)
{
    d->sendRequest("04 00 51 00"_hex, [this, callback](auto message) {
        if (message.lanMessageId() == LanMessageId::GetBroadcastFlags && message.length() >= 8) {
            const auto subscriptions = Subscriptions{qFromLittleEndian<quint32>(message.lanData())};

            d->setSubscriptions(subscriptions);
            callIfDefined(callback, subscriptions);
            return true;
        }

        return false;
    });
}

void Client::querySerialNumber(std::function<void(quint32)> callback)
{
    d->sendRequest("04 00 10 00"_hex, [this, callback](auto message) {
        if (message.lanMessageId() == LanMessageId::GetSerialNumber && message.length() >= 8) {
            const auto serialNumber = qFromLittleEndian<quint32>(message.lanData());

            d->setSerialNumber(serialNumber);
            callIfDefined(callback, serialNumber);
            return true;
        }

        return false;
    });
}

void Client::queryFirmwareVersion(std::function<void(QVersionNumber)> callback)
{
    d->sendRequest("07 00 40 00 f1 0a fb"_hex, [this, callback](auto message) {
        if (message.xbusMessageId() == XBusMessageId::GetFirmwareVersionReply && message.length() >= 9) {
            const auto majorVersion = fromBCD(message.xbusData()[1]);
            const auto minorVersion = fromBCD(message.xbusData()[2]);
            const auto version = QVersionNumber{majorVersion, minorVersion};

            d->setFirmwareVersion(version);
            callIfDefined(callback, version);
            return true;
        }

        return false;
    });
}

void Client::queryXBusVersion(std::function<void(QVersionNumber, quint8)> callback)
{
    d->sendRequest("07 00 40 00 21 21 00"_hex, [this, callback](auto message) {
        if (message.xbusMessageId() == XBusMessageId::GetVersionReply && message.length() >= 9) {
            const auto rawVersion = message.xbusData()[1];
            const auto centralId = message.xbusData()[2];
            const auto version = QVersionNumber{rawVersion >> 4, rawVersion & 15};

            d->setProtocolVersion(version);
            d->setCentralId(centralId);
            callIfDefined(callback, version, centralId);
            return true;
        }

        return false;
    });
}

void Client::queryCentralStatus(std::function<void()> callback)
{
    d->sendRequest("04 00 85 00"_hex, [this, callback](auto message) {
        if (d->parseSystemStateDataChanged(message)) {
            callIfDefined(callback);
            return true;
        }

        return false;
    });
}

void Client::queryHardwareInfo(std::function<void(HardwareType, QVersionNumber)> callback)
{
    d->sendRequest("04 00 1A 00"_hex, [this, callback](auto message) {
        if (message.lanMessageId() == LanMessageId::GetHardwareInfo && message.length() >= 12) {
            const auto hardwareType = static_cast<HardwareType>(qFromLittleEndian<quint32>(message.lanData()));
            d->setHardwareType(hardwareType);

            const auto majorVersion = fromBCD(message.lanData()[5]);
            const auto minorVersion = fromBCD(message.lanData()[4]);
            const auto version = QVersionNumber{majorVersion, minorVersion};

            d->setFirmwareVersion(version);
            callIfDefined(callback, hardwareType, version);
            return true;
        }

        return false;
    });
}

void Client::queryLockState(std::function<void(LockState)> callback)
{
    d->sendRequest("04 00 18 00"_hex, [this, callback](auto message) {
        if (message.lanMessageId() == LanMessageId::GetLockState && message.length() >= 5) {
            const auto lockState = static_cast<LockState>(message.lanData()[0]);

            d->setLockState(lockState);
            callIfDefined(callback, lockState);
            return true;
        }

        return false;
    });
}

void Client::queryVehicle(quint16 address, std::function<void(VehicleInfo)> callback)
{
    // BUG: Apparently this gains acquires ownership of the vehicle; we do not want this
    auto request = "09 00 40 00 E3 F0 00 00 00"_hex;
    qToBigEndian<quint16>(address & 0x3fff, request.data() + 6);
    updateChecksum(&request);

    d->sendRequest(std::move(request), [this, callback](auto message) {
        if (const auto info = d->parseVehicleInfo(std::move(message))) {
            callIfDefined(callback, info.value());
            emit vehicleInfoReceived(info.value());
            return true;
        }

        return false;
    });
}

void Client::queryVehicleAny()
{
    d->sendRequest("09 00 40 00 E3 F0"_hex, {});
}

void Client::queryAccessoryInfo(dcc::AccessoryAddress address, std::function<void (AccessoryInfo)> callback)
{
    auto request = "09 00 40 00 44 00 00 00 00"_hex;
    qToBigEndian<quint16>((address - 1) & 0x3fff, request.data() + 5);
    updateChecksum(&request);

    d->sendRequest(std::move(request), [this, address, callback](auto message) {
        if (const auto info = d->parseAccessoryInfo(std::move(message))) {
            if (info->address() == address)
                callIfDefined(callback, info.value());

            emit accessoryInfoReceived(info.value());
            return true;
        }

        return false;
    });
}

void Client::queryTurnoutInfo(dcc::AccessoryAddress address, std::function<void (TurnoutInfo)> callback)
{
    auto request = "08 00 40 00 43 00 00 00"_hex;
    qToBigEndian<quint16>((address - 1) & 0x3fff, request.data() + 5);
    updateChecksum(&request);

    d->sendRequest(std::move(request), [this, address, callback](auto message) {
        if (const auto info = d->parseTurnoutInfo(std::move(message))) {
            if (info->address() == address)
                callIfDefined(callback, info.value());

            emit turnoutInfoReceived(info.value());
            return true;
        }

        return false;
    });
}

void Client::queryDetectorInfo(rm::DetectorAddress address, std::function<void(QList<core::DetectorInfo>)> callback)
{
    switch (address.type()) {
    case rm::DetectorAddress::Type::RBusGroup:
    case rm::DetectorAddress::Type::RBusModule:
    case rm::DetectorAddress::Type::RBusPort:
        queryRBusDetectorInfo(address.rbusGroup(), [callback](RBusDetectorInfo info) {
            // FIXME: try filtering for the requested module and port
            callIfDefined(callback, static_cast<QList<core::DetectorInfo>>(info));
        });

        break;

    case rm::DetectorAddress::Type::LoconetSIC:
        Q_UNIMPLEMENTED();
//        queryLoconetDetectorInfo(0x80, 0, std::move(callback));
        break;

    case rm::DetectorAddress::Type::LoconetModule:
        Q_UNIMPLEMENTED();
//        queryLoconetDetectorInfo(0x81, address.reportAddress, std::move(callback));
        break;

    case rm::DetectorAddress::Type::LissyModule:
        Q_UNIMPLEMENTED();
//        queryLoconetDetectorInfo(0x82, address.lissyAddress, std::move(callback));
        break;

    case rm::DetectorAddress::Type::CanNetwork:
    case rm::DetectorAddress::Type::CanModule:
    case rm::DetectorAddress::Type::CanPort:
        queryCanDetectorInfo(address.canNetwork(), [callback](QList<CanDetectorInfo> info) {
            // FIXME: try filtering for the requested module and port
            callIfDefined(callback, {CanDetectorInfo::merge(std::move(info))});
        });

        break;

    case rm::DetectorAddress::Type::Invalid:
        break;
    }
}

void Client::queryRBusDetectorInfo(rm::rbus::GroupId group, std::function<void(RBusDetectorInfo)> callback)
{
    auto request = "05 00 81 00 00"_hex;
    qToBigEndian(static_cast<quint8>(group), request.data() + 4);
    // there is no checksum in this request

    d->sendRequest(std::move(request), [this, group, callback](auto message) {
        if (const auto info = d->parseRBusDetectorInfo(std::move(message))) {
            if (info->group() == group) {
                callIfDefined(callback, info.value());
                return true;
            }
        }

        return false;
    });
}

void Client::queryLoconetDetectorInfo(LoconetDetectorInfo::Query type, quint16 address,
                                      std::function<void (QList<LoconetDetectorInfo>)> callback)
{
    if (type == LoconetDetectorInfo::Query::Invalid) // FIXME: softassert
        return;

    if (type == LoconetDetectorInfo::Query::Report)
        --address;

    auto request = "07 00 A4 00 00 00 00"_hex;
    qToBigEndian<quint8>(core::value(type), request.data() + 4);
    qToBigEndian<quint16>(address, request.data() + 5);
    // there is no checksum in this request

    d->queueLoconetDetectorInfoCallback(type, address, std::move(callback));
    d->sendRequest(std::move(request), {});
}

void Client::queryCanDetectorInfo(rm::can::NetworkId networkId, std::function<void(QList<CanDetectorInfo>)> callback)
{
    auto request = "07 00 C4 00 00 00 00"_hex;
    qToLittleEndian<quint16>(networkId, request.data() + 5);
    // there is no checksum in this request

    d->queueCanDetectorInfoCallback(networkId, std::move(callback));
    d->sendRequest(std::move(request), {});
}

void Client::queryRailcom(quint16 address, std::function<void (RailcomInfo)> callback)
{
    auto request = "07 00 89 00 01 00 00"_hex;
    qToBigEndian<quint16>(address & 0x3fff, request.data() + 5);
    updateChecksum(&request);

    d->sendRequest(std::move(request), [this, address, callback](auto message) {
        if (const auto info = d->parseRailcomInfo(std::move(message))) {
            if (info->address() == address) {
                callIfDefined(callback, info.value());
                emit railcomInfoReceived(info.value());
                return true;
            }
        }

        return false;
    });
}

void Client::queryRailcomAny()
{
    d->sendRequest("04 00 89 00"_hex, {});
}

void Client::sendRequest(QByteArray request)
{
    d->sendRequest(std::move(request), {});
}

void Client::readVariable(quint16 address, quint16 index, std::function<void(Error, quint8)> callback)
{
    qCInfo(lcRequests, "readVariable: address=%d, index=%d", address, index);

    auto request = QByteArray{};

    if (address > 0) {
        // read in POM mode
        request = "0C 00 40 00 E6 30 00 00 00 00 00 00"_hex;
        qToBigEndian<quint16>(address & 0x3fff, request.data() + 6);
        qToBigEndian<quint16>((static_cast<quint16>(index - 1) & 0x03ff) | 0xe400, request.data() + 8);
        updateChecksum(&request);
    } else {
        // read in direct mode
        request = "09 00 40 00 23 11 00 00 00"_hex;
        qToBigEndian<quint16>(index - 1, request.data() + 6);
        updateChecksum(&request);
    }

    d->startProgrammingTimeout([this, address, index, callback] {
        qCWarning(d->logger()) << TimeoutError << "for reading CV" << index;

        if (address == 0)
            enableTrackPower(); // FIXME: do not do this unconditionally: track power only should be restored if we had it before. also we should probably enable it at beginning if missing

        callIfDefined(callback, TimeoutError, {});
    });

    d->sendRequest(std::move(request), [this, address, index, callback](auto message) {
        const auto result = d->parseProgrammingResponse(message);

        if (std::holds_alternative<Error>(result)) {
            qCWarning(d->logger()) << std::get<Error>(result) << "for reading CV" << index;

            d->stopProgrammingTimeout();

            if (address == 0)
                enableTrackPower();


            callIfDefined(callback, std::get<Error>(result), {});
            return true;
        } else if (std::holds_alternative<ConfigurationVariable>(result)) {
            d->stopProgrammingTimeout();

            if (const auto cv = std::get<ConfigurationVariable>(result); cv.index == index) {
                if (address == 0)
                    enableTrackPower(); // FIXME: do not do this unconditionally: track power only should be restored if we had it before. also we should probably enable it at beginning if missing

                callIfDefined(callback, NoError, cv.value);
                return true;
            }
        }

        return false;
    });
}

void Client::writeVariable(quint16 address, quint16 index, quint8 value, std::function<void(Error, quint8)> callback)
{
    qCInfo(lcRequests, "writeVariable: address=%d, index=%d, value=%d", address, index, value);

    auto request = QByteArray{};

    if (address > 0) {
        // write in POM mode
        request = "0C 00 40 00 E6 30 00 00 00 00 00 00"_hex;
        qToBigEndian<quint16>(address & 0x3fff, request.data() + 6);
        qToBigEndian<quint16>((static_cast<quint16>(index - 1) & 0x03ff) | 0xec00, request.data() + 8);
        qToBigEndian<quint8>(value, request.data() + 10);
        updateChecksum(&request);
    } else {
        // write in direct mode
        request = "0a 00 40 00 24 12 00 00 00 00"_hex;
        qToBigEndian<quint16>(index - 1, request.data() + 6);
        qToBigEndian<quint8>(value, request.data() + 8);
        updateChecksum(&request);
    }

    d->sendRequest(std::move(request), {});

    // the Z21 doesn't give any feedback (anymore?) when writing variables with firmware 1.42
    QTimer::singleShot(100ms, this, [this, address, index, value, callback] {
        readVariable(address, index, [value, callback](Error error, quint8 verifiedValue) {
            if (error != Error::NoError)
                callIfDefined(callback, error, {});
            else if (verifiedValue != value)
                callIfDefined(callback, Error::ValueRejectedError, {});
            else
                callIfDefined(callback, Error::NoError, verifiedValue);
        });
    });

    /*
    d->sendRequest(std::move(request), [this, address, index, callback](auto message) {
        qInfo() << "writeVariable observer, processing:" << message.rawData().toHex(' ');
        if (const auto result = d->parseProgrammingResponse(message);
                std::holds_alternative<Error>(result)) {
            qInfo() << "writeVariable observer, error:" << std::get<Error>(result);
            if (address == 0)
                enableTrackPower(); // FIXME: do not do this unconditionally: track power only should be restored if we had it before. also we should probably enable it at beginning if missing

            callIfDefined(callback, std::get<Error>(result), {});
            return true;
        } else if (std::holds_alternative<ConfigurationVariable>(result)) {
            qInfo() << "writeVariable observer, variable" << std::get<ConfigurationVariable>(result).index;
            if (const auto cv = std::get<ConfigurationVariable>(result); cv.index == index) {
                if (address == 0)
                    enableTrackPower(); // FIXME: do not do this unconditionally: track power only should be restored if we had it before. also we should probably enable it at beginning if missing

                callIfDefined(callback, NoError, cv.value);
                return true;
            }
        } else {
            qInfo() << "writeVariable observer, unknown";
        }

        return false;
    });
     */
}

void Client::readVariables(quint16 address, QList<quint16> indices,
                           std::function<void(Error, quint16, quint8)> callback)
{
    if (!indices.isEmpty()) {
        const auto variable = indices.takeFirst();

        readVariable(address, variable, [=, this](auto error, auto value) {
            callIfDefined(callback, error, variable, value);
            readVariables(address, indices, callback);
        });
    }
}

void Client::startDetectorProgramming(rm::rbus::ModuleId module)
{
    d->startFeedbackModuleProgramming(module);
}

void Client::stopDetectorProgramming()
{
    d->stopFeedbackModuleProgramming();
}

// slots //

void Client::connectToHost(Subscriptions subscriptions, QHostAddress host, quint16 port)
{
    d->connectToHost(std::move(host), port);

    subscribe(subscriptions);
    queryTrackStatus([this](auto) {
        d->stopConnectTimeout();
        emit connected();

        // FIXME: maybe only do for black Z21
        queryCanDetectorInfo(rm::can::NetworkIdAny, {});
    });
}

void Client::connectToHost(Subscriptions subscriptions, QString host, quint16 port)
{
    connectToHost(subscriptions, QHostAddress{host}, port);
}

void Client::disconnectFromHost()
{
    d->disconnectFromHost();
}

QDebug operator<<(QDebug debug, const AccessoryInfo &info)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "address=" << info.address()
          << ", state=" << info.state();

    return debug;
}

QDebug operator<<(QDebug debug, const RBusDetectorInfo &info)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "module=" << info.group()
          << ", occupancy=" << info.occupancy();

    return debug;
}

QDebug operator<<(QDebug debug, const LoconetDetectorInfo &info)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "address=" << info.address()
          << ", type=" << info.type()
          << ", occupancy=" << info.occupancy()
          << ", vehicle=" << info.vehicle()
          << ", direction=" << info.direction()
          << ", lissyClass=" << info.lissyClass()
          << ", lissySpeed=" << info.lissySpeed();

    return debug;
}

QDebug operator<<(QDebug debug, const CanDetectorInfo &info)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "networkId=0x" << Qt::hex << info.networkId()
          << ", module=" << Qt::dec << info.module()
          << ", port=" << info.port()
          << ", type=" << info.type();

    if (info.isOccupancy()) {
        debug << ", occupancy=" << info.occupancy()
              << ", powerState=" << info.powerState();
    } else if (info.isLastVehicleSet()) {
        debug << ", vehicle1=" << info.vehicle1()
              << ", direction1=" << info.direction1()
              << ", vehicle2=" << info.vehicle2()
              << ", direction2=" << info.direction2();
    } else {
        debug << ", value1=" << info.value1()
              << ", value2=" << info.value2();
    }

    return debug;
}

QDebug operator<<(QDebug debug, const LibraryInfo &info)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "index=" << info.index()
          << ", address=" << info.address()
          << ", flags=" << info.flags()
          << ", name=" << info.name();

    return debug;
}

QDebug operator<<(QDebug debug, const RailcomInfo &info)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);

    if (info.isValid()) {
        debug << "address=" << info.address()
              << ", received=" << info.receiveCounter()
              << ", errors=" << info.errorCounter()
              << ", options=" << info.options()
              << ", speed=" << info.speed()
              << ", qos=" << info.qos();
    }

    return debug;
}

QDebug operator<<(QDebug debug, const TurnoutInfo &info)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "address=" << info.address()
          << ", state=" << info.state();

    return debug;
}

QDebug operator<<(QDebug debug, const VehicleInfo &info)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(info)>{debug};

    debug.setVerbosity(0);
    debug << "address=" << info.address()
          << ", received=" << info.protocol()
          << ", speed=" << info.speed()
          << ", functions=" << info.functions()
          << ", acquired=" << info.acquired()
          << ", direction=" << info.direction()
          << ", consistMode=" << info.consistMode()
          << ", smartSearch=" << info.smartSearch();

    return debug;
}

} // namespace lmrs::roco::z21

#include "moc_z21client.cpp"
