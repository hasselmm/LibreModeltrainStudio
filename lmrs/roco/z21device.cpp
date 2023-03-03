#include "z21device.h"

#include "z21client.h"

#include <lmrs/core/accessories.h>
#include <lmrs/core/algorithms.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/memory.h>
#include <lmrs/core/parameters.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>
#include <lmrs/core/validatingvariantmap.h>
#include <lmrs/core/vehicleinfomodel.h>

#include <QBitArray>
#include <QHostAddress>
#include <QLoggingCategory>
#include <QNetworkInterface>
#include <QTimer> // FIXME
#include <QVersionNumber>

namespace lmrs::roco::z21 {

namespace {

using namespace accessory;
namespace dcc = core::dcc;

using std::chrono::milliseconds;

constexpr auto s_parameter_address = "address"_BV;

auto device(Client *client) { return core::checked_cast<Device *>(client->parent()); }

// =====================================================================================================================

class AccessoryControl : public core::AccessoryControl
{
public:
    explicit AccessoryControl(Client *parent);

    core::Device *device() const override { return z21::device(client()); }

    Features features() const override;

    void setAccessoryState(dcc::AccessoryAddress address, quint8 state) override;
    void setTurnoutState(dcc::AccessoryAddress address, dcc::TurnoutState state, bool enabled = true) override;
    void setTurnoutState(dcc::AccessoryAddress address, dcc::TurnoutState state, milliseconds duration) override;
    void requestEmergencyStop() override;

    void requestAccessoryInfo(dcc::AccessoryAddress address, AccessoryInfoCallback callback) override;
    void requestTurnoutInfo(dcc::AccessoryAddress address, TurnoutInfoCallback callback) override;

private:
    Client *client() const { return core::checked_cast<Client *>(parent()); }

    void onAccessoryInfoReceived(AccessoryInfo info);
    void onTurnoutInfoReceived(TurnoutInfo info);
};

// =====================================================================================================================

class DebugControl : public core::DebugControl
{
public:
    explicit DebugControl(Client *parent);

    core::Device *device() const override { return z21::device(client()); }

    Features features() const noexcept override { return Feature::NativeFrames; }
    void sendDccFrame(QByteArray frame, DccPowerMode powerMode, DccFeedbackMode feedbackMode) const override;
    void sendNativeFrame(QByteArray nativeFrame) const override;

    QString nativeProtocolName() const noexcept override { return tr("Z21 Protocol"); }
    QList<QPair<QString, QByteArray>> nativeExampleFrames() const noexcept override;

private:
    Client *client() const { return core::checked_cast<Client *>(parent()); }
};

// =====================================================================================================================

class DetectorControl : public core::DetectorControl
{
public:
    explicit DetectorControl(Client *parent);

    core::Device *device() const override { return z21::device(client()); }

private:
    Client *client() const { return core::checked_cast<Client *>(parent()); }

    void onDetectorInfoReceived(QList<DetectorInfo> infoList);

private:
    QHash<DetectorAddress, DetectorInfo> m_infoCache;
};

// =====================================================================================================================

class PowerControl : public core::PowerControl
{
public:
    explicit PowerControl(Client *parent);

    core::Device *device() const override { return z21::device(client()); }
    State state() const override { return state(client()->trackStatus()); }

    void enableTrackPower(core::ContinuationCallback<core::Error> callback) override;
    void disableTrackPower(core::ContinuationCallback<core::Error> callback) override;

    static State state(Client::TrackStatus trackStatus);

private:
    Client *client() const { return core::checked_cast<Client *>(parent()); }
};

// =====================================================================================================================

class VariableControl : public core::VariableControl
{
public:
    explicit VariableControl(Client *parent);

    core::Device *device() const override { return z21::device(client()); }
    Features features() const override { return Feature::DirectProgramming | Feature::ProgrammingOnMain; }

    void readVariable(dcc::VehicleAddress address, dcc::VariableIndex variable,
                      core::ContinuationCallback<VariableValueResult> callback) override;
    void writeVariable(dcc::VehicleAddress address, dcc::VariableIndex variable, dcc::VariableValue value,
                       core::ContinuationCallback<VariableValueResult> callback) override;

private:
    Client *client() const { return core::checked_cast<Client *>(parent()); }
};

// =====================================================================================================================

class VehicleControl : public core::VehicleControl
{
public:
    explicit VehicleControl(Client *parent);

    core::Device *device() const override { return z21::device(client()); }

    void subscribe(dcc::VehicleAddress address, SubscriptionType type) override;

    void setSpeed(dcc::VehicleAddress address, dcc::Speed speed, dcc::Direction direction) override;
    void setFunction(dcc::VehicleAddress address, dcc::Function function, bool enabled) override;

private:
    Client *client() const {
//        qInfo() << Q_FUNC_INFO;
//        qInfo() << "this:" << this;
//        qInfo() << "parent():" << parent();
        return core::checked_cast<Client *>(parent());
    }

    void onVehicleInqueryInterval();

    void onLibraryInfoReceived(z21::LibraryInfo info);
    void onRailcomInfoReceived(z21::RailcomInfo info);
    void onVehicleInfoReceived(z21::VehicleInfo info);

    QList<dcc::VehicleAddress> m_subscriptions;
    dcc::VehicleAddress m_currentVehicle;
    qsizetype m_inqueryPointer = 0;

    QSet<dcc::VehicleAddress> m_knownVehicles;
    core::ConstPointer<QTimer> m_vehicleInqueryTimer{this};
};

// =====================================================================================================================

auto defaultAddresses()
{
    QList<QHostAddress> defaultAddresses;

    for (const auto &hostAddress: QNetworkInterface::allAddresses()) {
        if (hostAddress.protocol() != QAbstractSocket::IPv4Protocol)
            continue;
        if (hostAddress.isLoopback())
            continue;

        // return active interface address with last octet replaced by 111
        auto deviceAddress = QHostAddress{(hostAddress.toIPv4Address() & 0xffffff00) | 111};

        if (!defaultAddresses.contains(deviceAddress))
            defaultAddresses.append(std::move(deviceAddress));
    }

    if (defaultAddresses.isEmpty())
        defaultAddresses.append(QHostAddress{0xc0'a8'00'6f}); // 192.168.0.111

    return defaultAddresses;
}

// =====================================================================================================================

AccessoryControl::AccessoryControl(Client *parent)
    : core::AccessoryControl{parent}
{
    connect(client(), &Client::accessoryInfoReceived, this, &AccessoryControl::onAccessoryInfoReceived);
    connect(client(), &Client::turnoutInfoReceived, this, &AccessoryControl::onTurnoutInfoReceived);
}

AccessoryControl::Features AccessoryControl::features() const
{
    return Feature::Turnouts | Feature::Signals | Feature::Durations | Feature::EmergencyStop;
}

void AccessoryControl::setAccessoryState(dcc::AccessoryAddress address, quint8 state)
{
    client()->setAccessoryState(address, state);
}

void AccessoryControl::setTurnoutState(dcc::AccessoryAddress address, dcc::TurnoutState state, bool enabled)
{
    switch (state) {
    case dcc::TurnoutState::Straight:
        client()->setTurnoutState(address, z21::Client::Straight, enabled);
        break;

    case dcc::TurnoutState::Branched:
        client()->setTurnoutState(address, z21::Client::Branching, enabled);
        break;

    case dcc::TurnoutState::Unknown:
    case dcc::TurnoutState::Invalid:
        break;
    }
}

void AccessoryControl::setTurnoutState(dcc::AccessoryAddress address, dcc::TurnoutState state, milliseconds duration)
{
    switch (state) {
    case dcc::TurnoutState::Straight:
        client()->setTurnoutState(address, z21::Client::Straight, duration);
        break;

    case dcc::TurnoutState::Branched:
        client()->setTurnoutState(address, z21::Client::Branching, duration);
        break;

    case dcc::TurnoutState::Unknown:
    case dcc::TurnoutState::Invalid:
        break;
    }
}

void AccessoryControl::requestEmergencyStop()
{
    client()->requestAccessoryStop();
}

void AccessoryControl::requestAccessoryInfo(dcc::AccessoryAddress address, AccessoryInfoCallback callback)
{
    client()->queryAccessoryInfo(address, [callback](auto info) {
        core::callIfDefined(std::move(callback), {info.address(), info.state()});
    });
}

void AccessoryControl::requestTurnoutInfo(dcc::AccessoryAddress address, TurnoutInfoCallback callback)
{
    client()->queryTurnoutInfo(address, [callback](auto info) {
        core::callIfDefined(std::move(callback), {info.address(), info.state()});
    });
}

void AccessoryControl::onAccessoryInfoReceived(AccessoryInfo info)
{
    qCDebug(logger(this)) << info;
    emit accessoryInfoChanged({info.address(), info.state()});
}

void AccessoryControl::onTurnoutInfoReceived(TurnoutInfo info)
{
    qCDebug(logger(this)) << info;
    emit turnoutInfoChanged({info.address(), info.state()});
}

// =====================================================================================================================

DebugControl::DebugControl(Client *parent)
    : core::DebugControl{parent}
{}

void DebugControl::sendDccFrame(QByteArray /*frame*/, DccPowerMode /*powerMode*/, DccFeedbackMode /*feedbackMode*/) const
{
    Q_UNIMPLEMENTED();
}

void DebugControl::sendNativeFrame(QByteArray nativeFrame) const
{
    qInfo() << "NATIVE FRAME" << nativeFrame.toHex(' ');
    client()->sendRequest(std::move(nativeFrame));
    qInfo() << "NATIVE FRAME";
}

QList<QPair<QString, QByteArray>> DebugControl::nativeExampleFrames() const noexcept
{
    return {
        {tr("Query Railcom information for DCC#2280"), "07 00 89 00 01 08 e8 e0"_hex},
    };
}

// =====================================================================================================================

DetectorControl::DetectorControl(Client *parent)
    : core::DetectorControl{parent}
{
    connect(client(), &Client::detectorInfoReceived, this, &DetectorControl::onDetectorInfoReceived);
}

void DetectorControl::onDetectorInfoReceived(QList<DetectorInfo> infoList)
{
    for (const auto &info: infoList) {
        if (const auto oldInfo = std::exchange(m_infoCache[info.address()], info); oldInfo != info)
            emit detectorInfoChanged(info);
    }
}

// =====================================================================================================================

PowerControl::PowerControl(Client *parent)
    : core::PowerControl{parent}
{
    connect(client(), &Client::trackStatusChanged, this, [this](auto trackStatus) {
        emit stateChanged(state(trackStatus));
    });
}

void PowerControl::enableTrackPower(core::ContinuationCallback<core::Error> callback)
{
    client()->enableTrackPower([this, callback](auto trackStatus) {
        const auto error = core::makeError(trackStatus == Client::TrackStatus::PowerOn);
        switch (core::callIfDefined(core::Continuation::Proceed, callback, error)) {
        case core::Continuation::Retry:
            if (const auto next = callback.retry()) {
                qCWarning(core::logger(this), "Retrying to enable track power (%d/%d)",
                          callback.retryCount(), callback.retryLimit());
                enableTrackPower(next);
            }

            break;

        case core::Continuation::Proceed:
        case core::Continuation::Abort:
            break;
        }
    });
}

void PowerControl::disableTrackPower(core::ContinuationCallback<core::Error> callback)
{
    client()->disableTrackPower([this, callback](auto trackStatus) {
        const auto error = core::makeError(trackStatus == Client::TrackStatus::PowerOff);
        switch (core::callIfDefined(core::Continuation::Proceed, callback, error)) {
        case core::Continuation::Retry:
            if (const auto next = callback.retry()) {
                qCWarning(core::logger(this), "Retrying to disable track power (%d/%d)",
                          callback.retryCount(), callback.retryLimit());
                disableTrackPower(next);
            }

            break;

        case core::Continuation::Proceed:
        case core::Continuation::Abort:
            break;
        }
    });
}

// =====================================================================================================================

VariableControl::VariableControl(Client *parent)
    : core::VariableControl{parent}
{}

void VariableControl::readVariable(dcc::VehicleAddress address, dcc::VariableIndex variable,
                                   core::ContinuationCallback<VariableValueResult> callback)
{
    // FIXME: use lmrs types for z21::Client
    client()->readVariable(address, variable, [=, this](auto error, auto value) {
        const auto genericError = static_cast<core::Error>(error); // FIXME: do proper conversion, not just a cast
        switch (core::callIfDefined(core::Continuation::Proceed, callback, {genericError, value})) {
        case core::Continuation::Retry:
            if (const auto next = callback.retry()) {
                qCWarning(core::logger(this), "Retrying to read variable %d for address %d (%d/%d)",
                          variable.value, address.value, callback.retryCount(), callback.retryLimit());
                readVariable(address, variable, next);
            }

            break;

        case core::Continuation::Proceed:
        case core::Continuation::Abort:
            break;
        }
    });
}

void VariableControl::writeVariable(dcc::VehicleAddress address, dcc::VariableIndex variable, dcc::VariableValue value,
                                    core::ContinuationCallback<VariableValueResult> callback)
{
    // FIXME: use lmrs types for z21::Client
    client()->writeVariable(address, variable, value, [=, this](auto error, auto verifiedValue) {
        const auto genericError = static_cast<core::Error>(error); // FIXME: do proper conversion, not just a cast
        switch (core::callIfDefined(core::Continuation::Proceed, callback, {genericError, verifiedValue})) {
        case core::Continuation::Retry:
            if (const auto next = callback.retry()) {
                qCWarning(core::logger(this), "Retrying to write variable %d for address %d (%d/%d)",
                          variable.value, address.value, callback.retryCount(), callback.retryLimit());
                writeVariable(address, variable, value, next);
            }

            break;

        case core::Continuation::Proceed:
        case core::Continuation::Abort:
            break;
        }
    });
}

// =====================================================================================================================

VehicleControl::VehicleControl(Client *parent)
    : core::VehicleControl{parent}
{
    m_vehicleInqueryTimer->setInterval(2s);
//    m_vehicleInqueryTimer->setInterval(200ms);

    connect(m_vehicleInqueryTimer, &QTimer::timeout, this, &VehicleControl::onVehicleInqueryInterval);

    connect(client(), &Client::connected, m_vehicleInqueryTimer.get(), qOverload<>(&QTimer::start));
    connect(client(), &Client::disconnected, m_vehicleInqueryTimer.get(), &QTimer::stop);

    connect(client(), &Client::libraryInfoReceived, this, &VehicleControl::onLibraryInfoReceived);
    connect(client(), &Client::railcomInfoReceived, this, &VehicleControl::onRailcomInfoReceived);
    connect(client(), &Client::vehicleInfoReceived, this, &VehicleControl::onVehicleInfoReceived);
}

void VehicleControl::subscribe(dcc::VehicleAddress address, SubscriptionType type)
{
    if (type == CancelSubscription)
        m_subscriptions.remove(address);
    else if (!m_subscriptions.contains(address))
        m_subscriptions.append(address);

    if (type == PrimarySubscription)
        m_currentVehicle = address;
}

void VehicleControl::setSpeed(dcc::VehicleAddress address, dcc::Speed speed, dcc::Direction direction)
{
    if (std::holds_alternative<dcc::Speed14>(speed))
        client()->setSpeed14(address, std::get<dcc::Speed14>(speed), direction);
    else if (std::holds_alternative<dcc::Speed28>(speed))
        client()->setSpeed28(address, std::get<dcc::Speed28>(speed), direction);
    else
        client()->setSpeed126(address, speedCast<dcc::Speed126>(speed), direction);

    m_currentVehicle = address;
}

void VehicleControl::setFunction(dcc::VehicleAddress address, dcc::Function function, bool enabled)
{
    if (enabled)
        client()->enableFunction(address, function);
    else
        client()->disableFunction(address, function);

    m_currentVehicle = address;
}

void VehicleControl::onVehicleInqueryInterval()
{
    QList<dcc::VehicleAddress> nextQueries;

    // always query the prioritized current vehicle
    if (m_currentVehicle)
        nextQueries.append(m_currentVehicle);

    // now add up to five vehicles we didn't query recently
    if (!m_subscriptions.isEmpty()) {
        for (const auto n = m_subscriptions.size(), end = m_inqueryPointer + n;
             m_inqueryPointer < end && nextQueries.size() < 5; ++m_inqueryPointer) {
            if (const auto address = m_subscriptions[m_inqueryPointer % n]; !nextQueries.contains(address))
                nextQueries.append(address);
        }

        m_inqueryPointer %= m_subscriptions.size();
    }

    // now query the vehicles the app is interested in
    for (const auto address: nextQueries) {
        client()->queryVehicle(address);
        client()->queryRailcom(address);
    }

    client()->queryVehicleAny();  // FIXME: this doesn't seem to work
    client()->queryRailcomAny();
}

void VehicleControl::onLibraryInfoReceived(LibraryInfo info)
{
    qDebug(logger(this)) << info;
    m_knownVehicles.insert(info.address());

    emit vehicleNameChanged(info.address(), info.name());
}

void VehicleControl::onRailcomInfoReceived(RailcomInfo info)
{
    qDebug(logger(this)) << info;

    if (!m_knownVehicles.contains(info.address())) {
        m_knownVehicles.insert(info.address());
        emit vehicleInfoChanged(core::VehicleInfo{info.address(), {}, {}});
    }
}

void lmrs::roco::z21::VehicleControl::onVehicleInfoReceived(z21::VehicleInfo info)
{
    qDebug(logger(this)) << info;
    m_knownVehicles.insert(info.address());

    dcc::FunctionState functionState; // FIXME: All this conversion quite a mess: We should replace z21::VehicleInfo by core::VehicleInfo
    core::VehicleInfo::Flags flags;

    for (auto fn: QMetaTypeId<z21::VehicleInfo::Function>{})
        functionState[static_cast<size_t>(fn.index())] = info.functions() & fn.value();

    emit vehicleInfoChanged(core::VehicleInfo{info.address(), info.direction(), info.speed(), std::move(functionState), flags});
}

PowerControl::State PowerControl::state(Client::TrackStatus trackStatus)
{
    switch (trackStatus) {
    case Client::TrackStatus::EmergencyStop:
        return State::EmergencyStop;
    case Client::TrackStatus::ShortCircuit:
        return State::ShortCircuit;
    case Client::TrackStatus::ProgrammingMode:
        return State::ServiceMode;
    case Client::TrackStatus::PowerOn:
        return State::PowerOn;
    case Client::TrackStatus::PowerOff:
        break;
    };

    return State::PowerOff;
}

} // namespace

// =====================================================================================================================

class Device::Private : public core::PrivateObject<Device>
{
public:
    explicit Private(QHostAddress address, DeviceFactory *factory, Device *parent)
        : PrivateObject{parent}
        , address{address}
        , factory{factory}
    {}

    void updateDeviceInfo();

    bool isUnlocked() const
    {
        return client->lockState() == Client::LockState::NoLock
                || client->lockState() == Client::LockState::StartUnlocked;
    }

    template<typename T>
    T unlockedOnly(T value) const
    {
        if (isUnlocked())
            return value;

        return {};
    }

    void onConnected();
    void onDisconnected();

    QHostAddress address;

    core::ConstPointer<DeviceFactory> factory;
    core::ConstPointer<Client> client{q()};
    core::ConstPointer<AccessoryControl> accessoryControl{client};
    core::ConstPointer<DebugControl> debugControl{client};
    core::ConstPointer<DetectorControl> detectorControl{client};
    core::ConstPointer<PowerControl> powerControl{client};
    core::ConstPointer<VariableControl> variableControl{client};
    core::ConstPointer<VehicleControl> vehicleControl{client};

    QHash<core::DeviceInfo, QVariant> deviceInfo;
};

void Device::Private::updateDeviceInfo()
{
    client->queryXBusVersion();
    client->queryFirmwareVersion();
    client->queryHardwareInfo();
    client->querySerialNumber();
    client->queryTrackStatus();
    client->queryCentralStatus();
    client->queryLockState();
    client->querySubscriptions();
}

void Device::Private::onConnected()
{
    const auto guard = core::propertyGuard(q(), &Device::state, &Device::stateChanged);
    q()->setDeviceInfo(core::DeviceInfo::ManufacturerId, 161); // FIXME: provide manufacturer constants
    updateDeviceInfo();
}

void Device::Private::onDisconnected()
{
    emit q()->stateChanged(q()->state());
}

// =====================================================================================================================

Device::Device(QHostAddress address, DeviceFactory *factory, QObject *parent)
    : core::Device{parent}
    , d{std::move(address), factory, this}
{
    connect(d->client, &Client::connected, d, &Private::onConnected);
    connect(d->client, &Client::disconnected, d, &Private::onDisconnected);
    connect(d->client, &Client::isConnectedChanged, this, [this] { emit stateChanged(state()); });
    connect(d->client, &Client::subscriptionsChanged, this, [this] { emit stateChanged(state()); });
    connect(d->client, &Client::lockStateChanged, this, &Device::controlsChanged);

    // FIXME: why are these two responses missing sometimes?
    // z21.stream: received 14 00 84 00 05 00 03 00 13 00 1b 00 83 45 83 45 02 00 03 00
    // z21.stream: received 08 00 51 00 01 01 01 00

    observe(core::DeviceInfo::DeviceAddress, d->client.get(), &Client::hostAddress, &Client::hostAddressChanged);
    observe(core::DeviceInfo::DevicePort, d->client.get(), &Client::hostPort, &Client::hostPortChanged);
    observe(core::DeviceInfo::TrackStatus, d->client.get(), &Client::trackStatus, &Client::trackStatusChanged, &PowerControl::state);
    observe(core::DeviceInfo::DeviceStatus, d->client.get(), &Client::centralStatus, &Client::centralStatusChanged);
    observe(core::DeviceInfo::Capabilities, d->client.get(), &Client::capabilities, &Client::capabilitiesChanged);

    observe(core::DeviceInfo::ProductId, d->client.get(), &Client::hardwareType, &Client::hardwareTypeChanged);
    observe(core::DeviceInfo::FirmwareVersion, d->client.get(), &Client::firmwareVersion, &Client::firmwareVersionChanged);
    observe(core::DeviceInfo::HardwareLock, d->client.get(), &Client::lockState, &Client::lockStateChanged);
    observe(core::DeviceInfo::SerialNumber, d->client.get(), &Client::serialNumber, &Client::serialNumberChanged);
    observe(core::DeviceInfo::ProtocolVersion, d->client.get(), &Client::protocolVersion, &Client::protocolVersionChanged);
    observe(core::DeviceInfo::ProtocolClientId, d->client.get(), &Client::centralId, &Client::centralIdChanged);

    observe(core::DeviceInfo::MainTrackCurrent, d->client.get(), &Client::mainTrackCurrent, &Client::mainTrackCurrentChanged);
    observe(core::DeviceInfo::ProgrammingTrackCurrent, d->client.get(), &Client::programmingTrackCurrent, &Client::programmingTrackCurrentChanged);
    observe(core::DeviceInfo::MainTrackCurrentFiltered, d->client.get(), &Client::filteredMainTrackCurrent, &Client::filteredMainTrackCurrentChanged);
    observe(core::DeviceInfo::SupplyVoltage, d->client.get(), &Client::supplyVoltage, &Client::supplyVoltageChanged);
    observe(core::DeviceInfo::MainTrackVoltage, d->client.get(), &Client::trackVoltage, &Client::trackVoltageChanged);
    observe(core::DeviceInfo::Temperature, d->client.get(), &Client::temperature, &Client::temperatureChanged);
}

Device::State Device::state() const
{
    if (!d->client->isConnected())
        return State::Disconnected;
    if (d->deviceInfo.isEmpty() || !d->client->subscriptions())
        return State::Connecting;

    return State::Connected;
}

QString Device::name() const
{
    auto productName = core::coalesce(deviceInfoText(core::DeviceInfo::ProductId), tr("Z21 Command Station"));
    return tr("%1 at %2").arg(std::move(productName), d->address.toString());
}

QString Device::uniqueId() const
{
    return "roco:z21:"_L1 + QString::number(d->client->serialNumber());
}

bool Device::connectToDevice()
{
    const auto guard = core::propertyGuard(this, &Device::state, &Device::stateChanged);

    d->client->connectToHost(Client::Subscription::Generic
                             | Client::Subscription::AnyVehicle
                             | Client::Subscription::Railcom
                             | Client::Subscription::RailcomAny
                             | Client::Subscription::RBus
                             | Client::Subscription::CanDetector
                             | Client::Subscription::LoconetDetector
                             | Client::Subscription::SystemState,
                             d->address);

    return true;
}

void Device::disconnectFromDevice()
{
    const auto guard = core::propertyGuard(this, &Device::state, &Device::stateChanged);
    d->client->disconnectFromHost();
}

core::DeviceFactory *Device::factory()
{
    return d->factory;
}

core::AccessoryControl *Device::accessoryControl()
{
    if (d->client->capabilities() & Client::Capability::AccessoryControl)
        return d->unlockedOnly(d->accessoryControl.get());

    return {};
}

core::DebugControl *Device::debugControl()
{
    return d->debugControl;
}

core::DetectorControl *Device::detectorControl()
{
    if (d->client->capabilities() & Client::Capability::DetectorControl)
        return d->detectorControl;

    return {};
}

core::PowerControl *Device::powerControl()
{
    return d->powerControl;
}

core::VariableControl *Device::variableControl()
{
    return d->variableControl;
}

core::VehicleControl *Device::vehicleControl()
{
    if (d->client->capabilities() & Client::Capability::VehicleControl)
        return d->unlockedOnly(d->vehicleControl.get());

    return {};
}

QVariant Device::deviceInfo(core::DeviceInfo id, int role) const
{
    if (role == Qt::EditRole) {
        const auto info = d->deviceInfo[id];

        if (info.typeId() == qMetaTypeId<Client::HardwareType>())
            return QVariant::fromValue(core::value(info.value<Client::HardwareType>()));
        if (info.typeId() == qMetaTypeId<QHostAddress>())
            return info.value<QHostAddress>().toString();

        return info;
    }

    if (role == Qt::DisplayRole) {
        if (id == core::DeviceInfo::ProductId)
            return Client::hardwareName(d->deviceInfo[id].value<Client::HardwareType>());
    }

    if (role == Qt::UserRole)
        return d->deviceInfo[id];

    return {};
}

void Device::updateDeviceInfo()
{
    d->updateDeviceInfo();
}

void Device::setDeviceInfo(core::DeviceInfo id, QVariant value)
{
    if (auto it = d->deviceInfo.find(id); it == d->deviceInfo.end()) {
        d->deviceInfo.insert(id, std::move(value));
        emit deviceInfoChanged({id});
    } else if (*it != value) {
        *it = std::move(value);
        emit deviceInfoChanged({id});
    }
}

QString DeviceFactory::name() const
{
    return tr("Z21 Command Station");
}

QList<core::Parameter> DeviceFactory::parameters() const
{
    return {
        core::Parameter::hostAddress(s_parameter_address, tr("Host &address:"), defaultAddresses()),
    };
}

core::Device *DeviceFactory::create(QVariantMap parameters, QObject *parent)
{
    const auto parameterSet = core::ValidatingVariantMap<DeviceFactory>{std::move(parameters)};

    if (const auto address = parameterSet.find<QHostAddress>(s_parameter_address))
        return new Device{address.value(), this, parent};

    if (const auto addressString = parameterSet.find<QString>(s_parameter_address)) {
        if (auto address = QHostAddress{addressString.value()}; !address.isNull())
            return new Device{std::move(address), this, parent};

        qCWarning(core::logger(this), "Parameter \"%s\" has unsupported value \"%ls\"",
                  s_parameter_address.data(), qUtf16Printable(addressString.value()));

    }

    return {};
}

} // namespace lmrs::roco::z21
