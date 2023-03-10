#ifndef LMRS_ROCO_Z21_CLIENT_H
#define LMRS_ROCO_Z21_CLIENT_H

#include <lmrs/core/detectors.h>
#include <lmrs/core/quantities.h>

#include <QObject>

class QHostAddress;
class QVersionNumber;

namespace lmrs::roco::z21 {

Q_NAMESPACE

namespace accessory = core::accessory;
namespace dcc = core::dcc;

enum class LanMessageId : quint16 {
    GetSerialNumber = 0x10,
    GetLockState = 0x18,
    GetHardwareInfo = 0x1a,
    XNetMessage = 0x40,
    GetBroadcastFlags = 0x51,
    RBusDetectorDataChanged = 0x80,
    SystemStateDataChanged = 0x84,
    RailcomDataChanged = 0x88,
    LoconetDetectorDataChanged = 0xa4,
    CanDetectorDataChanged = 0xc4,
};

Q_ENUM_NS(LanMessageId)

enum class XBusMessageId : quint16 {
    TurnoutInfo                 = 0x43,     // LAN_X_TURNOUT_INFO
    AccessoryInfo               = 0x44,     // LAN_X_EXT_ACCESSORY_INFO
    BroadcastPowerOff           = 0x61'00,  // LAN_X_TRACK_POWER_OFF
    BroadcastPowerOn            = 0x61'01,  // LAN_X_TRACK_POWER_ON
    BroadcastProgrammingMode    = 0x61'02,  // LAN_X_BC_PROGRAMMING_MODE
    BroadcastShortCircuit       = 0x61'08,  // LAN_X_BC_TRACK_SHORT_CIRCUIT
    ConfigErrorShortCircuit     = 0x61'12,  // LAN_X_CV_NACK_SC
    ConfigErrorValueRejected    = 0x61'13,  // LAN_X_CV_NACK
    UnknownCommand              = 0x61'82,  // LAN_X_UNKNOWN_COMMAND
    ConfigResult                = 0x64'14,  // LAN_X_CV_RESULT
    BroadcastEmergencyStop      = 0x81'00,  // LAN_X_BC_STOPPED
    StatusChanged               = 0x62'22,  // LAN_X_STATUS_CHANGED
    VehicleInfo                 = 0xef,     // LAN_X_LOCO_INFO
    LibraryInfo                 = 0xea'f1,  // ???

    GetVersionRequest           = 0x21'21,  // LAN_X_GET_VERSION
    GetVersionReply             = 0x63'21,  // LAN_X_GET_VERSION
    GetFirmwareVersionRequest   = 0xf1'0a,  // LAN_X_GET_FIRMWARE_VERSION
    GetFirmwareVersionReply     = 0xf3'0a,  // LAN_X_GET_FIRMWARE_VERSION
    Invalid                     = 0,
};

Q_ENUM_NS(XBusMessageId)

class Message
{
public:
    Message(QByteArray data)
        : m_data{std::move(data)}
    {}

    [[nodiscard]] int length() const;
    [[nodiscard]] auto rawData() const { return m_data; }

    // Z21 LAN protocol
    [[nodiscard]] LanMessageId lanMessageId() const;
    [[nodiscard]] auto lanMessageLength() const { return length() - 4; }
    [[nodiscard]] auto lanData() const { return reinterpret_cast<const quint8 *>(m_data.constData() + 4); }

    // XBus protocol
    [[nodiscard]] XBusMessageId xbusMessageId() const;
    [[nodiscard]] auto xbusMessageLength() const { return length() - 5; }
    [[nodiscard]] auto xbusData() const { return reinterpret_cast<const quint8 *>(m_data.constData() + 5); }

private:
    QByteArray m_data;
};

class VehicleInfo
{
    Q_GADGET

public:
    enum Protocol { DCC14 = 0, DCC28 = 2, DCC126 = 4 }; // FIXME: replace by dcc type
    Q_ENUM(Protocol)

    enum Function { // FIXME: replace by dcc type
        F0 = (1 << 4),
        F1 = (1 << 0),
        F2 = (1 << 1),
        F3 = (1 << 2),
        F4 = (1 << 3),

        F5 = (1 << 8),
        F6 = (1 << 9),
        F7 = (1 << 10),
        F8 = (1 << 11),
        F9 = (1 << 12),
        F10 = (1 << 13),
        F11 = (1 << 14),
        F12 = (1 << 15),

        F13 = (1 << 16),
        F14 = (1 << 17),
        F15 = (1 << 18),
        F16 = (1 << 19),
        F17 = (1 << 20),
        F18 = (1 << 21),
        F19 = (1 << 22),
        F20 = (1 << 23),

        F21 = (1 << 24),
        F22 = (1 << 25),
        F23 = (1 << 26),
        F24 = (1 << 27),
        F25 = (1 << 28),
        F26 = (1 << 29),
        F27 = (1 << 30),
        F28 = (1 << 31),
    };

    Q_FLAG(Function)
    Q_DECLARE_FLAGS(Functions, Function)

    explicit VehicleInfo(QByteArray data);

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] dcc::VehicleAddress address() const;
    [[nodiscard]] Protocol protocol() const;
    [[nodiscard]] dcc::Speed speed() const;
    [[nodiscard]] dcc::Direction direction() const;
    [[nodiscard]] Functions functions() const;

    [[nodiscard]] bool acquired() const;
    [[nodiscard]] bool consistMode() const;
    [[nodiscard]] bool smartSearch() const;

private:
    QByteArray m_data;
};

class AccessoryInfo
{
    Q_GADGET

public:
    explicit AccessoryInfo(QByteArray data);

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] dcc::AccessoryAddress address() const;
    [[nodiscard]] dcc::AccessoryState state() const;

private:
    QByteArray m_data;
};

class TurnoutInfo
{
    Q_GADGET

public:
    explicit TurnoutInfo(QByteArray data);

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] dcc::AccessoryAddress address() const;
    [[nodiscard]] dcc::TurnoutState state() const;

private:
    QByteArray m_data;
};

class RailcomInfo
{
    Q_GADGET

public:
    enum class Option {
        Speed1    = 0x01,   // CH7 subindex 0
        Speed2    = 0x02,   // CH7 subindex 1
        QoS       = 0x04,   // CH7 subindex 7
        Reserved3 = 0x08,
        Reserved4 = 0x10,
        Reserved5 = 0x20,
        Reserved6 = 0x40,
        Reserved7 = 0x80,
    };

    Q_ENUM(Option)
    Q_DECLARE_FLAGS(Options, Option)

    RailcomInfo() = default;
    explicit RailcomInfo(QByteArray data);

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] quint16 address() const;
    [[nodiscard]] quint32 receiveCounter() const;
    [[nodiscard]] quint16 errorCounter() const;
    [[nodiscard]] Options options() const;
    [[nodiscard]] quint8 speed() const;
    [[nodiscard]] quint8 qos() const;

private:
    QByteArray m_data;
};

struct RBusDetectorInfo
{
    Q_GADGET

public:
    explicit RBusDetectorInfo(QByteArray data = {});

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] accessory::rbus::GroupId group() const;
    [[nodiscard]] QBitArray occupancy() const;

    [[nodiscard]] operator QList<accessory::DetectorInfo>() const;
    [[nodiscard]] bool operator==(const RBusDetectorInfo &rhs) const noexcept = default;
    [[nodiscard]] auto data() const { return m_data; }

private:
    QByteArray m_data;
};

struct LoconetDetectorInfo
{
    Q_GADGET

public:
    enum class Query : quint8
    {
        Invalid = 0,
        SIC = 0x80,
        Report = 0x81,
        Lissy = 0x82,
    };

    enum class Type : quint8
    {
        Occupancy =         0x01,
        BlockEnter =        0x02,
        BlockLeave =        0x03,
        LissyAddress =      0x10,
        LissyOccupancy =    0x11,
        LissySpeed =        0x12,
    };

    Q_ENUM(Type)

    using Occupancy = accessory::DetectorInfo::Occupancy;

    explicit LoconetDetectorInfo(QByteArray data = {});

    [[nodiscard]] bool isValid() const;

    [[nodiscard]] Type type() const;

    [[nodiscard]] accessory::loconet::ReportAddress address() const;
    [[nodiscard]] Occupancy occupancy() const;
    [[nodiscard]] dcc::VehicleAddress vehicle() const;
    [[nodiscard]] dcc::Direction direction() const;

    [[nodiscard]] quint8 lissyClass() const;
    [[nodiscard]] quint16 lissySpeed() const;

    [[nodiscard]] static QList<accessory::DetectorInfo> merge(QList<LoconetDetectorInfo> infoList);
    [[nodiscard]] bool operator==(const LoconetDetectorInfo &rhs) const noexcept = default;
    [[nodiscard]] auto data() const { return m_data; }

    [[nodiscard]] static std::pair<Query, quint16> address(const accessory::DetectorAddress &address);

private:
    QByteArray m_data;
};

struct CanDetectorInfo
{
    Q_GADGET

public:
    enum class Type : quint8
    {
        Occupancy   = 0x01,
        VehicleSet1 = 0x11,
        VehicleSet2,
        VehicleSet3,
        VehicleSet4,
        VehicleSet5,
        VehicleSet6,
        VehicleSet7,
        VehicleSet8,
        VehicleSet9,
        VehicleSet10,
        VehicleSet11,
        VehicleSet12,
        VehicleSet13,
        VehicleSet14,
        VehicleSet15,
        VehicleSetFirst = VehicleSet1,
        VehicleSetLast = VehicleSet15,
    };

    Q_ENUM(Type)

    static constexpr core::Range<Type> VehicleSetRange = {Type::VehicleSetFirst, Type::VehicleSetLast};

    using Occupancy = accessory::DetectorInfo::Occupancy;
    using PowerState = accessory::DetectorInfo::PowerState;

    explicit CanDetectorInfo(QByteArray data = {});

    [[nodiscard]] bool isValid() const;

    using Key = std::tuple<accessory::can::NetworkId, accessory::can::ModuleId, accessory::can::PortIndex>;
    [[nodiscard]] auto key() const { return Key{networkId(), module(), port()}; }

    [[nodiscard]] accessory::can::NetworkId networkId() const;
    [[nodiscard]] accessory::can::ModuleId module() const;
    [[nodiscard]] accessory::can::PortIndex port() const;

    [[nodiscard]] Type type() const;

    [[nodiscard]] quint16 value1() const;
    [[nodiscard]] quint16 value2() const;

    [[nodiscard]] Occupancy occupancy() const;
    [[nodiscard]] PowerState powerState() const;

    [[nodiscard]] auto vehicle1() const { return vehicle(value1()); }
    [[nodiscard]] auto vehicle2() const { return vehicle(value2()); }

    [[nodiscard]] auto direction1() const { return direction(value1()); }
    [[nodiscard]] auto direction2() const { return direction(value2()); }

    [[nodiscard]] QList<dcc::VehicleAddress> vehicles() const;
    [[nodiscard]] QList<dcc::Direction> directions() const;

    [[nodiscard]] auto isOccupancy() const noexcept { return type() == Type::Occupancy; }
    [[nodiscard]] auto isVehicleSet() const noexcept { return VehicleSetRange.contains(type()); }
    [[nodiscard]] auto isLastVehicleSet() const noexcept { return isVehicleSet() && vehicle2() == 0; }

    [[nodiscard]] static QList<accessory::DetectorInfo> merge(QList<CanDetectorInfo> infoList);
    [[nodiscard]] bool operator==(const CanDetectorInfo &rhs) const noexcept = default;
    [[nodiscard]] auto data() const { return m_data; }

private:
    [[nodiscard]] dcc::VehicleAddress vehicle(quint16 value) const;
    [[nodiscard]] dcc::Direction direction(quint16 value) const;

    QByteArray m_data;
};

class LibraryInfo
{
    Q_GADGET

public:
    using Protocol = VehicleInfo::Protocol;

    LibraryInfo() = default;
    explicit LibraryInfo(QByteArray data);

    [[nodiscard]] quint16 address() const;
    [[nodiscard]] quint8 index() const;
    [[nodiscard]] quint8 flags() const;
    [[nodiscard]] QString name() const;

private:
    QByteArray m_data;
};

class Client : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isConnected READ isConnected NOTIFY isConnectedChanged FINAL)
    Q_PROPERTY(QHostAddress hostAddress READ hostAddress NOTIFY hostAddressChanged FINAL)
    Q_PROPERTY(quint16 hostPort READ hostPort NOTIFY hostPortChanged FINAL)
    Q_PROPERTY(TrackStatus trackStatus READ trackStatus NOTIFY trackStatusChanged FINAL)
    Q_PROPERTY(CentralStatus centralStatus READ centralStatus NOTIFY centralStatusChanged FINAL)
    Q_PROPERTY(Subscriptions subscriptions READ subscriptions NOTIFY subscriptionsChanged FINAL)
    Q_PROPERTY(quint32 serialNumber READ serialNumber NOTIFY serialNumberChanged FINAL)
    Q_PROPERTY(QVersionNumber firmwareVersion READ firmwareVersion NOTIFY firmwareVersionChanged FINAL)
    Q_PROPERTY(QVersionNumber protocolVersion READ protocolVersion NOTIFY protocolVersionChanged FINAL)
    Q_PROPERTY(int centralId READ centralId NOTIFY centralIdChanged FINAL)
    Q_PROPERTY(lmrs::core::milliamperes mainTrackCurrent READ mainTrackCurrent NOTIFY mainTrackCurrentChanged FINAL)
    Q_PROPERTY(lmrs::core::milliamperes programmingTrackCurrent READ programmingTrackCurrent NOTIFY programmingTrackCurrentChanged FINAL)
    Q_PROPERTY(lmrs::core::milliamperes filteredMainTrackCurrent READ filteredMainTrackCurrent NOTIFY filteredMainTrackCurrentChanged FINAL)
    Q_PROPERTY(lmrs::core::celsius temperature READ temperature NOTIFY temperatureChanged FINAL)
    Q_PROPERTY(lmrs::core::millivolts supplyVoltage READ supplyVoltage NOTIFY supplyVoltageChanged FINAL)
    Q_PROPERTY(lmrs::core::millivolts trackVoltage READ trackVoltage NOTIFY trackVoltageChanged FINAL)
    Q_PROPERTY(HardwareType hardwareType READ hardwareType NOTIFY hardwareTypeChanged FINAL)
    Q_PROPERTY(LockState lockState READ lockState NOTIFY lockStateChanged FINAL)

public:
    enum class TrackStatus {
        PowerOn                 = 0x00,
        EmergencyStop           = 0x01,
        PowerOff                = 0x02,
        ShortCircuit            = 0x04,
        ProgrammingMode         = 0x20,
    };

    Q_ENUM(TrackStatus)

    enum class CentralStatusFlag {
        NoError                 = 0x00,
        HighTemperature         = 0x01,
        PowerLost               = 0x02,
        ShortCircuitExternal    = 0x04,
        ShortCircuitInternal    = 0x08,
        RCN213                  = 0x20,
    };

    Q_FLAG(CentralStatusFlag)
    Q_DECLARE_FLAGS(CentralStatus, CentralStatusFlag)

    enum class Capability {
        DCC                 = 0x01,
        MM                  = 0x02,
        Reserved2           = 0x04,
        Railcom             = 0x08,
        VehicleControl      = 0x10,
        AccessoryControl    = 0x20,
        DetectorControl     = 0x40,
        UnlockCodeNeeded    = 0x80,
    };

    Q_FLAG(Capability)
    Q_DECLARE_FLAGS(Capabilities, Capability)

    enum class Subscription {
        Generic             = 0x00000001,
        RBus                = 0x00000002,
        Railcom             = 0x00000004,
        SystemState         = 0x00000100,
        AnyVehicle          = 0x00010000,   // since 1.20, changed with 1.24
        CanBooster          = 0x00020000,   // since 1.41
        RailcomAny          = 0x00040000,   // since 1.29
        CanDetector         = 0x00080000,   // since 1.30
        LoconetBus          = 0x01000000,   // since 1.20
        LoconetVehicle      = 0x02000000,   // since 1.20
        LoconetTurnout      = 0x04000000,   // since 1.20
        LoconetDetector     = 0x08000000,   // since 1.22
    };

    Q_FLAG(Subscription);
    Q_DECLARE_FLAGS(Subscriptions, Subscription);

    enum class HardwareType : quint16 {
        Unknown             = 0x0000,

        Z21Old              = 0x0200,
        Z21New              = 0x0201,
        SmartRail           = 0x0202,
        Z21Small            = 0x0203,
        Z21Start            = 0x0204,

        SingleBooster       = 0x0205,
        DualBooster         = 0x0206,

        Z21XL               = 0x0211,
        XLBooster           = 0x0212,

        Z21SwitchDecoder    = 0x0301,
        Z21SignalDecoder    = 0x0302,
    };

    Q_ENUM(HardwareType)

    enum class LockState {
        Invalid         = -1,
        NoLock          = 0x00,
        StartLocked     = 0x01,
        StartUnlocked   = 0x02,
    };

    Q_ENUM(LockState)

    enum Error {
        NoError,
        UnknownCommandError,
        ValueRejectedError,
        ShortCircuitError,
        TimeoutError,
    };

    Q_ENUM(Error)

    static constexpr quint16 DefaultPort = 21105;

    // lifetime
    explicit Client(QObject *parent = {});

    // attributes
    [[nodiscard]] QHostAddress hostAddress() const;
    [[nodiscard]] quint16 hostPort() const;
    [[nodiscard]] bool isConnected() const;

    [[nodiscard]] TrackStatus trackStatus() const;
    [[nodiscard]] CentralStatus centralStatus() const;
    [[nodiscard]] Capabilities capabilities() const;
    [[nodiscard]] Subscriptions subscriptions() const;
    [[nodiscard]] quint32 serialNumber() const;
    [[nodiscard]] QVersionNumber firmwareVersion() const;
    [[nodiscard]] QVersionNumber protocolVersion() const;
    [[nodiscard]] int centralId() const;
    [[nodiscard]] core::milliamperes mainTrackCurrent() const;
    [[nodiscard]] core::milliamperes programmingTrackCurrent() const;
    [[nodiscard]] core::milliamperes filteredMainTrackCurrent() const;
    [[nodiscard]] core::celsius temperature() const;
    [[nodiscard]] core::millivolts supplyVoltage() const;
    [[nodiscard]] core::millivolts trackVoltage() const;
    [[nodiscard]] HardwareType hardwareType() const;
    [[nodiscard]] LockState lockState() const;

    [[nodiscard]] static QString hardwareName(Client::HardwareType type);

    // operations
    void disableTrackPower(std::function<void(TrackStatus state)> callback = {});
    void enableTrackPower(std::function<void(TrackStatus state)> callback = {});
    void requestEmergencyStop(std::function<void(TrackStatus state)> callback = {});
    void subscribe(Subscriptions subscriptions);
    void logoff();

    void setSpeed14(dcc::VehicleAddress address, dcc::Speed14 speed, dcc::Direction direction);
    void setSpeed28(dcc::VehicleAddress address, dcc::Speed28 speed, dcc::Direction direction);
    void setSpeed126(dcc::VehicleAddress address, dcc::Speed126 speed, dcc::Direction direction);

    enum SetFunctionMode { DisableFunction = 0, EnableFunction = 1, ToggleFunction = 2 };
    void setFunction(dcc::VehicleAddress address, dcc::Function function, SetFunctionMode mode);
    void disableFunction(dcc::VehicleAddress address, dcc::Function function) { setFunction(address, function, DisableFunction); }
    void enableFunction(dcc::VehicleAddress address, dcc::Function function) { setFunction(address, function, EnableFunction); }
    void toggleFunction(dcc::VehicleAddress address, dcc::Function function) { setFunction(address, function, ToggleFunction); }


    enum TurnoutDirection { Straight = 0, Branching = 1 };
    void setAccessoryState(dcc::AccessoryAddress address, quint8 state);
    void setTurnoutState(dcc::AccessoryAddress address, TurnoutDirection direction, bool enabled); // FIXME: use proper state type
    void setTurnoutState(dcc::AccessoryAddress address, TurnoutDirection direction, std::chrono::milliseconds duration); // FIXME: use proper state type
    void requestAccessoryStop();

    void readVariable(quint16 address, quint16 index,
                      std::function<void(Error error, quint8 value)> callback = {}); // FIXME: use DCC types
    void writeVariable(quint16 address, quint16 index, quint8 value,
                       std::function<void(Error error, quint8 value)> callback = {}); // FIXME: use DCC types
    void readVariables(quint16 address, QList<quint16> indices,
                       std::function<void(Error error, quint16 variable, quint8 value)> callback = {}); // FIXME: use DCC types

    void startDetectorProgramming(accessory::rbus::ModuleId module);
    void stopDetectorProgramming();

    // queries
    void queryTrackStatus(std::function<void(TrackStatus state)> callback = {});
    void querySubscriptions(std::function<void(Subscriptions subscriptions)> callback = {});
    void querySerialNumber(std::function<void(quint32 serialNumber)> callback = {});
    void queryFirmwareVersion(std::function<void(QVersionNumber firmwareVersion)> callback = {});
    void queryXBusVersion(std::function<void(QVersionNumber protocolVersion, quint8 centralId)> callback = {});
    void queryCentralStatus(std::function<void()> callback = {});
    void queryHardwareInfo(std::function<void(HardwareType hardwareType, QVersionNumber firmwareVersion)> callback = {});
    void queryLockState(std::function<void(LockState lockState)> callback = {});
    void queryVehicle(quint16 address, std::function<void(VehicleInfo info)> callback = {}); // FIXME: use DCC types
    void queryVehicleAny();
    void queryAccessoryInfo(dcc::AccessoryAddress address, std::function<void (AccessoryInfo)> callback = {});
    void queryTurnoutInfo(dcc::AccessoryAddress address, std::function<void(TurnoutInfo)> callback = {});
    void queryDetectorInfo(accessory::DetectorAddress address, std::function<void (QList<accessory::DetectorInfo>)> callback = {});
    void queryRBusDetectorInfo(accessory::rbus::GroupId group, std::function<void(RBusDetectorInfo)> callback = {});
    void queryLoconetDetectorInfo(LoconetDetectorInfo::Query type, quint16 address,
                                  std::function<void(QList<LoconetDetectorInfo>)> callback = {});
    void queryCanDetectorInfo(accessory::can::NetworkId address, std::function<void (QList<CanDetectorInfo>)> callback = {});
    void queryRailcom(quint16 address, std::function<void(RailcomInfo info)> callback = {}); // FIXME: use DCC types
    void queryRailcomAny();

    // debugging
    void sendRequest(QByteArray request);

public slots:
    void connectToHost(lmrs::roco::z21::Client::Subscriptions subscriptions, QHostAddress host, quint16 port = DefaultPort);
    void connectToHost(lmrs::roco::z21::Client::Subscriptions subscriptions, QString host, quint16 port = DefaultPort);
    void disconnectFromHost();

signals:
    void errorOccured(lmrs::roco::z21::Client::Error error, QPrivateSignal);

    void connected(QPrivateSignal);
    void disconnected(QPrivateSignal);

    void accessoryInfoReceived(lmrs::roco::z21::AccessoryInfo info, QPrivateSignal);
    void railcomInfoReceived(lmrs::roco::z21::RailcomInfo info, QPrivateSignal);
    void detectorInfoReceived(QList<lmrs::core::accessory::DetectorInfo> info, QPrivateSignal);
    void rbusDetectorInfoReceived(lmrs::roco::z21::RBusDetectorInfo info, QPrivateSignal);
    void loconetDetectorInfoReceived(lmrs::roco::z21::LoconetDetectorInfo info, QPrivateSignal);
    void canDetectorInfoReceived(QList<lmrs::roco::z21::CanDetectorInfo> info, QPrivateSignal);
    void turnoutInfoReceived(lmrs::roco::z21::TurnoutInfo info, QPrivateSignal);
    void vehicleInfoReceived(lmrs::roco::z21::VehicleInfo info, QPrivateSignal);
    void libraryInfoReceived(lmrs::roco::z21::LibraryInfo info, QPrivateSignal);

    void isConnectedChanged(bool isConnected, QPrivateSignal);
    void hostAddressChanged(QHostAddress hostAddress, QPrivateSignal);
    void hostPortChanged(quint16 hostPort, QPrivateSignal);

    void trackStatusChanged(lmrs::roco::z21::Client::TrackStatus trackStatus, QPrivateSignal);
    void centralStatusChanged(lmrs::roco::z21::Client::CentralStatus centralStatus, QPrivateSignal);
    void capabilitiesChanged(lmrs::roco::z21::Client::Capabilities capabilities, QPrivateSignal);
    void subscriptionsChanged(lmrs::roco::z21::Client::Subscriptions subscriptions, QPrivateSignal);
    void serialNumberChanged(quint32 serialNumber, QPrivateSignal);
    void firmwareVersionChanged(QVersionNumber firmwareVersion, QPrivateSignal);
    void protocolVersionChanged(QVersionNumber protocolVersion, QPrivateSignal);
    void centralIdChanged(int centralId, QPrivateSignal);
    void mainTrackCurrentChanged(lmrs::core::milliamperes mainTrackCurrent, QPrivateSignal);
    void programmingTrackCurrentChanged(lmrs::core::milliamperes programmingTrackCurrent, QPrivateSignal);
    void filteredMainTrackCurrentChanged(lmrs::core::milliamperes filteredMainTrackCurrent, QPrivateSignal);
    void temperatureChanged(lmrs::core::celsius temperature, QPrivateSignal);
    void supplyVoltageChanged(lmrs::core::millivolts supplyVoltage, QPrivateSignal);
    void trackVoltageChanged(lmrs::core::millivolts trackVoltage, QPrivateSignal);
    void hardwareTypeChanged(lmrs::roco::z21::Client::HardwareType hardwareType, QPrivateSignal);
    void lockStateChanged(lmrs::roco::z21::Client::LockState lockState, QPrivateSignal);

private:
    class Private;

    Private *const d;
};

QDebug operator<<(QDebug debug, const AccessoryInfo &info);
QDebug operator<<(QDebug debug, const CanDetectorInfo &info);
QDebug operator<<(QDebug debug, const LibraryInfo &info);
QDebug operator<<(QDebug debug, const LoconetDetectorInfo &info);
QDebug operator<<(QDebug debug, const RBusDetectorInfo &info);
QDebug operator<<(QDebug debug, const RailcomInfo &info);
QDebug operator<<(QDebug debug, const TurnoutInfo &info);
QDebug operator<<(QDebug debug, const VehicleInfo &info);

} // namespace lmrs::roco::z21

Q_DECLARE_METATYPE(lmrs::roco::z21::Client::Subscriptions)
Q_DECLARE_OPERATORS_FOR_FLAGS(lmrs::roco::z21::Client::Subscriptions)

template<>
struct std::hash<lmrs::roco::z21::CanDetectorInfo::Key>
{
    size_t operator()(const lmrs::roco::z21::CanDetectorInfo::Key &key, size_t seed)
    {
        return qHashMulti(seed, get<0>(key), get<1>(key), get<2>(key));
    }
};

#endif // LMRS_ROCO_Z21_CLIENT_H
