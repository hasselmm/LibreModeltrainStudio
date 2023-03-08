#include "mx1device.h"

#include "mx1message.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/parameters.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/userliterals.h>
#include <lmrs/core/validatingvariantmap.h>
#include <lmrs/core/vehicleinfomodel.h>

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimerEvent>

namespace lmrs::zimo::mx1 {

namespace dcc = core::dcc;
using std::chrono::milliseconds;

namespace {

// =====================================================================================================================

constexpr auto s_parameter_portName = "port"_BV;
constexpr auto s_parameter_portSpeed = "speed"_BV;

// FIXME: share with esu::lp2
auto defaultPorts()
{
    auto serialPorts = QList<core::parameters::Choice>{};

    for (const auto &info: QSerialPortInfo::availablePorts()) {
        auto description = info.portName();

        if (auto text = info.description(); !text.isEmpty())
            description += " ("_L1 + text + ')'_L1;

        serialPorts.emplaceBack(std::move(description), info.portName());
    }

    return serialPorts;
}

auto serialSpeeds()
{
    const auto makeParameter = [](QSerialPort::BaudRate rate) {
        return core::parameters::Choice {
            DeviceFactory::tr("%L1 bit/s").arg(static_cast<int>(rate)),
            QVariant::fromValue(rate)
        };
    };

    return QList {
        makeParameter(QSerialPort::Baud1200),
        makeParameter(QSerialPort::Baud2400),
        makeParameter(QSerialPort::Baud4800),
        makeParameter(QSerialPort::Baud9600),
        makeParameter(QSerialPort::Baud19200),
        makeParameter(QSerialPort::Baud38400),
        makeParameter(QSerialPort::Baud115200), // seems to work via USB for MXUFL
    };
}

// ---------------------------------------------------------------------------------------------------------------------

struct RequestKey
{
    Request::SequenceNumber sequence;
    Request::Code code;

    constexpr RequestKey() noexcept = default;
    constexpr RequestKey(RequestKey &&) noexcept = default;
    constexpr RequestKey(const RequestKey &) noexcept = default;
    constexpr RequestKey(Request::SequenceNumber sequence, Request::Code code)
        : sequence{sequence}, code{code} {}

    RequestKey(const Request &request)
        : RequestKey{request.sequence(), request.code()} {}
    RequestKey(const Response &response)
        : RequestKey{response.requestSequence(), response.requestCode()} {}

    [[nodiscard]] constexpr auto operator<=>(const RequestKey &rhs) const noexcept = default;
};

inline size_t qHash(RequestKey key, size_t seed = 0)
{
    return qHashMulti(seed, key.sequence, core::value(key.code));
}

// =====================================================================================================================

} // namespace

// =====================================================================================================================

class Device::AccessoryControl : public core::AccessoryControl
{
public:
    explicit AccessoryControl(Private *d);

    core::Device *device() const override { return core::checked_cast<Device *>(parent()); }

    Features features() const override;
    void setAccessoryState(dcc::AccessoryAddress address, quint8 state) override;
    void setTurnoutState(dcc::AccessoryAddress address, dcc::TurnoutState state, bool enabled) override;
    void setTurnoutState(dcc::AccessoryAddress address, dcc::TurnoutState state, milliseconds duration) override;
    void requestAccessoryInfo(dcc::AccessoryAddress address, AccessoryInfoCallback callback) override;
    void requestTurnoutInfo(dcc::AccessoryAddress address, TurnoutInfoCallback callback) override;
    void requestEmergencyStop() override;

private:
    Private *d() const;
};

// =====================================================================================================================

class Device::DebugControl : public core::DebugControl
{
public:
    explicit DebugControl(Private *d);

    core::Device *device() const override { return core::checked_cast<Device *>(parent()); }

    Features features() const noexcept override { return Feature::NativeFrames; }
    QString nativeProtocolName() const noexcept override { return tr("Binary MX1 Protocol"); }
    QList<QPair<QString, QByteArray> > nativeExampleFrames() const noexcept override;
    void sendNativeFrame(QByteArray nativeFrame) const override;

private:
    Private *d() const;
};

// =====================================================================================================================

class Device::PowerControl : public core::PowerControl
{
public:
    explicit PowerControl(Private *d);

    core::Device *device() const override { return core::checked_cast<Device *>(parent()); }

    State state() const override { return currentState; }

    void enableTrackPower(core::ContinuationCallback<core::Error> callback) override;
    void disableTrackPower(core::ContinuationCallback<core::Error> callback) override;
    void updateState();

private:
    Private *d() const;

    State currentState = State::PowerOff;
};

// =====================================================================================================================

class Device::VariableControl : public core::VariableControl
{
public:
    explicit VariableControl(Private *d);

    core::Device *device() const override { return core::checked_cast<Device *>(parent()); }

    Features features() const override;

    void readVariable(dcc::VehicleAddress address, dcc::VariableIndex variable,
                      core::ContinuationCallback<VariableValueResult> callback) override;
    void writeVariable(dcc::VehicleAddress address, dcc::VariableIndex variable, dcc::VariableValue value,
                       core::ContinuationCallback<VariableValueResult> callback) override;

private:
    Private *d() const;
};

// =====================================================================================================================

class Device::VehicleControl : public core::VehicleControl
{
public:
    explicit VehicleControl(Private *d);

    core::Device *device() const override { return core::checked_cast<Device *>(parent()); }

    void subscribe(dcc::VehicleAddress address, SubscriptionType type) override;
    void setSpeed(dcc::VehicleAddress address, dcc::Speed speed, dcc::Direction direction) override;
    void setFunction(dcc::VehicleAddress address, dcc::Function function, bool enabled) override;

private:
    struct VehicleState {
        dcc::Speed speed;
        dcc::Direction direction;
        dcc::FunctionState functions;
        SubscriptionType subscription = SubscriptionType::CancelSubscription;
    };

    void updateVehicleState(dcc::VehicleAddress address, const VehicleState &vehicle);

    QHash<dcc::VehicleAddress, VehicleState> vehicles;

    Private *d() const;
};

// =====================================================================================================================

class Device::Private : public core::PrivateObject<Device>
{
public:
    explicit Private(QString portName, int speed, DeviceFactory *factory, Device *parent)
        : PrivateObject{parent}
        , serialPort{std::move(portName), this}
        , factory{factory}
        , speed{speed}
    {}

    StreamReader streamReader;
    StreamWriter streamWriter;

    core::ConstPointer<QSerialPort> serialPort;
    core::ConstPointer<DeviceFactory> factory;

    core::ConstPointer<AccessoryControl> accessoryControl{this};
    core::ConstPointer<DebugControl> debugControl{this};
    core::ConstPointer<PowerControl> powerControl{this};
    core::ConstPointer<VariableControl> variableControl{this};
    core::ConstPointer<VehicleControl> vehicleControl{this};

    using ObserverCallback = core::ContinuationCallback<Response>;

    struct PendingRequest {
        PendingRequest() noexcept = default;
        PendingRequest(Request request, ObserverCallback callback) noexcept
            : request{std::move(request)}
            , callback{std::move(callback)}
        {}

        Request request;
        ObserverCallback callback;
    };

    QList<PendingRequest> requestQueue;
    QHash<RequestKey, ObserverCallback> observers;

    const int speed;
    int connectTimeoutId = 0;

    QHash<core::DeviceInfo, QVariant> deviceInfo;

    void reportError(QString message);

    void timerEvent(QTimerEvent *event) override;
    void cancelConnectTimeout();

    bool connectToDevice();

    void setDeviceInfo(core::DeviceInfo id, QVariant value);
    void updateDeviceInfo();

    void onReadyRead();

    void queueRequest(Request request, ObserverCallback observer);
    void flushQueue();

    void handleResponse(Response response);

    void startCommunication();
    void queryStationEquipment();
    void queryStationStatus();
};

// =====================================================================================================================

Device::AccessoryControl::AccessoryControl(Private *d)
    : core::AccessoryControl{d->q()}
{}

Device::AccessoryControl::Features Device::AccessoryControl::features() const
{
    LMRS_UNIMPLEMENTED();
    return {};
}

void Device::AccessoryControl::setAccessoryState(dcc::AccessoryAddress /*address*/, quint8 /*state*/)
{
    LMRS_UNIMPLEMENTED();
}

void Device::AccessoryControl::setTurnoutState(dcc::AccessoryAddress /*address*/, dcc::TurnoutState /*state*/, bool /*enabled*/)
{
    LMRS_UNIMPLEMENTED();
}

void Device::AccessoryControl::setTurnoutState(dcc::AccessoryAddress /*address*/, dcc::TurnoutState /*state*/, milliseconds /*duration*/)
{
    LMRS_UNIMPLEMENTED();
}

void Device::AccessoryControl::requestAccessoryInfo(dcc::AccessoryAddress /*address*/, AccessoryInfoCallback /*callback*/)
{
    LMRS_UNIMPLEMENTED();
}

void Device::AccessoryControl::requestTurnoutInfo(dcc::AccessoryAddress /*address*/, TurnoutInfoCallback /*callback*/)
{
    LMRS_UNIMPLEMENTED();
}

void Device::AccessoryControl::requestEmergencyStop()
{
    LMRS_UNIMPLEMENTED();
}

// =====================================================================================================================

Device::DebugControl::DebugControl(Private *d)
    : core::DebugControl{d->q()}
{}

QList<QPair<QString, QByteArray>> Device::DebugControl::nativeExampleFrames() const noexcept
{
    return {
        {tr("Reset"), Request::reset().toData()},
        {tr("Power on"), Request::powerOn().toData()},
        {tr("Power off"), Request::powerOff().toData()},
        {tr("Query power state"), Request::queryPowerState().toData()},
        {tr("Query state of vehicle 800"), Request::queryVehicle(800).toData()},
        {tr("Query CV 29 of vehicle 800"), Request::readVariable(800, 29).toData()},
    };
}

void Device::DebugControl::sendNativeFrame(QByteArray nativeFrame) const
{
    // FIXME: generate sequence number, repair checksum
    d()->queueRequest(Request::fromData(std::move(nativeFrame)), {});
}

Device::Private *Device::DebugControl::d() const
{
    return core::checked_cast<Device *>(parent())->d;
}

// =====================================================================================================================

Device::PowerControl::PowerControl(Private *d)
    : core::PowerControl{d->q()}
{}

void Device::PowerControl::enableTrackPower(core::ContinuationCallback<core::Error> callback)
{
    d()->queueRequest(Request::powerOn(), [this, callback](Response response) {
        if (response.status() == Response::Status::Succeeded) {
            const auto guard = core::propertyGuard(this, &PowerControl::state, &PowerControl::stateChanged);
            currentState = PowerControl::State::PowerOn;
            callback(core::Error::NoError);
        } else {
            qCWarning(logger(this), "Could not enable track power: %s", core::key(response.status()));
            callback(core::Error::RequestFailed);
        }

        return core::Continuation::Done;
    });
}

void Device::PowerControl::disableTrackPower(core::ContinuationCallback<core::Error> callback)
{
    d()->queueRequest(Request::powerOff(), [this, callback](Response response) {
        if (response.status() == Response::Status::Succeeded) {
            const auto guard = core::propertyGuard(this, &PowerControl::state, &PowerControl::stateChanged);
            currentState = PowerControl::State::PowerOff;
            callback(core::Error::NoError);
        } else {
            qCWarning(logger(this), "Could not disable track power: %s", core::key(response.status()));
            callback(core::Error::RequestFailed);
        }

        return core::Continuation::Done;
    });
}

void Device::PowerControl::updateState()
{
    qInfo() << Q_FUNC_INFO;

    d()->queueRequest(Request::queryPowerState(), [this](PowerControlResponse response) {
        const auto guard = core::propertyGuard(this, &PowerControl::state, &PowerControl::stateChanged);

        if (response.status() & PowerControlResponse::Status::BroadcastStopped)
            currentState = PowerControl::State::EmergencyStop;
        else if (response.status() & PowerControlResponse::Status::TrackVoltage)
            currentState = PowerControl::State::PowerOn;
        else
            currentState = PowerControl::State::PowerOff;

        return core::Continuation::Done;
    });
}

// =====================================================================================================================

void Device::Private::reportError(QString message)
{
    qCWarning(logger(), "Error occured: %ls", qUtf16Printable(message));
    q()->disconnectFromDevice();
}

void Device::Private::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == connectTimeoutId) {
        cancelConnectTimeout();
        reportError(tr("Timeout reached while trying to connect"));
    }
}

void Device::Private::cancelConnectTimeout()
{
    if (const auto timerId = std::exchange(connectTimeoutId, 0))
        killTimer(timerId);
}

void Device::Private::queueRequest(Request request, ObserverCallback observer)
{
    requestQueue.emplaceBack(std::move(request), std::move(observer));

    if (requestQueue.size() == 1)
        flushQueue();
}

void Device::Private::flushQueue()
{
    if (requestQueue.isEmpty())
        return;

    auto [request, observer] = requestQueue.first();
    auto frame = request.toFrame();

    qCInfo(logger()).verbosity(QDebug::MinimumVerbosity)
            << "SEND:" << frame.toHex(' ') << request << request.hasValidChecksum()
            << Qt::hex << request.actualChecksum() << request.expectedChecksum();

    if (!streamWriter.writeFrame(std::move(frame))) {
        qCWarning(logger(), "Could not send frame: %ls", qUtf16Printable(streamWriter.errorString()));
        return;
    }

    if (observer)
        observers.insert(std::move(request), std::move(observer));

}

void Device::Private::onReadyRead()
{
    if (streamReader.readNext()) {
        auto frame = streamReader.frame();
        const auto message = Message::fromFrame(frame);

        qCInfo(logger()).verbosity(QDebug::MinimumVerbosity)
                << "RECV:" << frame.toHex(' ') << message;

        if (message.isValid()) {
            switch (message.type()) {
            case Message::Type::PrimaryResponse:
            case Message::Type::SecondaryResponse:
            case Message::Type::SecondaryAcknowledgement:
                handleResponse(std::move(message));
                break;

            case Message::Type::Request:
                qCWarning(logger(), "Unexpected message of type %s received (sequence=%d)",
                          core::key(message.type()), core::value(message.sequence()));
                break;
            }
        }
    }
}

void Device::Private::handleResponse(Response response)
{
    if (response.type() == Message::Type::PrimaryResponse
            && !requestQueue.isEmpty()) {
        requestQueue.removeFirst();
        flushQueue();
    }

    if (const auto it = observers.find(response); it != observers.end()) {
        qInfo().nospace() << "Calling observer for sequence="
                          << it.key().sequence << ", code="
                          << it.key().code;

        switch (std::invoke(*it, std::move(response))) {
        case core::Continuation::Abort:
            observers.erase(it);
            break;

        case core::Continuation::Retry:
            Q_UNREACHABLE(); // FIXME: resend request
            break;

        case core::Continuation::Proceed:
            break;
        }
    } else {
        qCWarning(logger(), "Could not find observer for %s response (sequence=%d)",
                  core::key(response.requestCode()), core::value(response.requestSequence()));
    }
}

void Device::Private::startCommunication()
{
    queueRequest(Request::reset(), [this](auto) {
        queueRequest(Request::startCommunication(), [this](SerialToolInfoReponse) {
            cancelConnectTimeout();

            const auto guard = core::propertyGuard(q(), &Device::state, &Device::stateChanged);
            setDeviceInfo(core::DeviceInfo::ManufacturerId, 145);
            updateDeviceInfo();

            return core::Continuation::Done;
        });

        return core::Continuation::Done;
    });
}

void Device::Private::queryStationEquipment()
{
    queueRequest(Request::queryStationEquipment(), [this](StationEquipmentReponse response) {
        qInfo() << response.dataSize();

        if (response.dataSize() > 1) {
            qInfo() << "romSize:" << response.romSize();
            qInfo() << "switches:" << response.switches();
            qInfo() << "target:" << response.target();

            if (const auto address = response.canAddress())
                setDeviceInfo(core::DeviceInfo::CanAddress, "0x"_L1 + QString::number(address, 16));

            setDeviceInfo(core::DeviceInfo::ProductId, QVariant::fromValue(response.deviceId()));
            setDeviceInfo(core::DeviceInfo::HardwareVersion, QVariant::fromValue(response.hardwareVersion()));
            setDeviceInfo(core::DeviceInfo::FirmwareVersion, QVariant::fromValue(response.firmwareVersion()));
            setDeviceInfo(core::DeviceInfo::FirmwareDate, QVariant::fromValue(response.firmwareDate()));

            if (const auto serialNumber = response.serialNumber())
                setDeviceInfo(core::DeviceInfo::SerialNumber, QVariant::fromValue(serialNumber.value()));
            if (const auto version = response.bootloaderVersion(); !version.isNull())
                setDeviceInfo(core::DeviceInfo::BootloaderVersion, QVariant::fromValue(version));

            return core::Continuation::Done;
        } else {
            qWarning("short response to QueryStationEquipment");
            return core::Continuation::Proceed;
        }
    });
}

void Device::Private::queryStationStatus()
{
    queueRequest(Request::queryStationStatus(), [this](StationStatusReponse response) {
        setDeviceInfo(core::DeviceInfo::MainTrackCurrent, QVariant::fromValue(response.current1()));
        setDeviceInfo(core::DeviceInfo::MainTrackVoltage, QVariant::fromValue(response.voltage1()));
        setDeviceInfo(core::DeviceInfo::ProgrammingTrackCurrent, QVariant::fromValue(response.current2()));
        setDeviceInfo(core::DeviceInfo::ProgrammingTrackVoltage, QVariant::fromValue(response.voltage2()));
        return core::Continuation::Done;
    });
}

Device::Private *Device::PowerControl::d() const
{
    return core::checked_cast<Device *>(parent())->d;
}

// =====================================================================================================================

Device::VariableControl::VariableControl(Private *d)
    : core::VariableControl{d->q()}
{}

Device::VariableControl::Features Device::VariableControl::features() const
{
    return Feature::DirectProgramming | Feature::ProgrammingOnMain;
}

void Device::VariableControl::readVariable(dcc::VehicleAddress address, dcc::VariableIndex variable,
                                           core::ContinuationCallback<VariableValueResult> callback)
{
    // FIXME: power control
    d()->queueRequest(Request::readVariable(address, variable),
                      [callback](VariableControlResponse response) {
        if (response.status() == Response::Status::Unknown) {
            callback({core::Error::NoError, response.value()});
            return core::Continuation::Done;
        } else if (response.status() != Response::Status::Succeeded) {
            callback({core::Error::RequestFailed, {}});
            return core::Continuation::Abort;
        } else {
            return core::Continuation::Proceed;
        }
    });
}

void Device::VariableControl::writeVariable(dcc::VehicleAddress address,
                                            dcc::VariableIndex variable, dcc::VariableValue value,
                                            core::ContinuationCallback<VariableValueResult> callback)
{
    // FIXME: power control
    d()->queueRequest(Request::writeVariable(address, variable, value),
                      [callback](VariableControlResponse response) {
        if (response.status() == Response::Status::Unknown) {
            callback({core::Error::NoError, response.value()});
            return core::Continuation::Done;
        } else if (response.status() != Response::Status::Succeeded) {
            callback({core::Error::RequestFailed, {}});
            return core::Continuation::Abort;
        } else {
            return core::Continuation::Proceed;
        }
    });
}

Device::Private *Device::VariableControl::d() const
{
    return core::checked_cast<Device *>(parent())->d;
}

// =====================================================================================================================

Device::VehicleControl::VehicleControl(Private *d)
    : core::VehicleControl{d->q()}
{}

void Device::VehicleControl::subscribe(dcc::VehicleAddress address, SubscriptionType type)
{
    vehicles[address].subscription = type;
}

void Device::VehicleControl::setSpeed(dcc::VehicleAddress address, dcc::Speed speed, dcc::Direction direction)
{
    // FIXME: power control
    auto &vehicleState = vehicles[address];
    vehicleState.direction = direction;
    vehicleState.speed = speed;

    updateVehicleState(address, vehicleState);
}

void Device::VehicleControl::setFunction(dcc::VehicleAddress address, dcc::Function function, bool enabled)
{
    // FIXME: power control
    auto &vehicleState = vehicles[address];
    vehicleState.functions.set(function.value, enabled);

    updateVehicleState(address, vehicleState);
}

void Device::VehicleControl::updateVehicleState(dcc::VehicleAddress address, const VehicleState &vehicle)
{
    d()->queueRequest(Request::vehicleControl(address, vehicle.speed, vehicle.direction, vehicle.functions), {});

    switch (vehicle.subscription) {
    case core::VehicleControl::NormalSubscription:
    case core::VehicleControl::PrimarySubscription:
        emit vehicleInfoChanged(core::VehicleInfo{address, vehicle.direction, vehicle.speed, vehicle.functions}, {});
        break;

    case core::VehicleControl::CancelSubscription:
        break;
    }
}

Device::Private *Device::VehicleControl::d() const
{
    return core::checked_cast<Device *>(parent())->d;
}

// =====================================================================================================================

Device::Device(QString portName, int portSpeed, DeviceFactory *factory, QObject *parent)
    : core::Device{parent}
    , d{std::move(portName), portSpeed, factory, this}
{
    observe(core::DeviceInfo::TrackStatus,
            static_cast<core::PowerControl *>(d->powerControl.get()),
            &core::PowerControl::state, &core::PowerControl::stateChanged);
}

Device::State Device::state() const
{
    if (!d->serialPort->isOpen())
        return State::Disconnected;
    if (d->deviceInfo.isEmpty())
        return State::Connecting;

    return State::Connected;
}

QString Device::name() const
{
    return tr("ZIMO Commmand Station at %1").arg(d->serialPort->portName());
}

QString Device::uniqueId() const
{
    // FIXME: how do we read the devices' serial number?
    return "zimo:mx1:"_L1 + d->serialPort->portName();
}

bool Device::Private::connectToDevice()
{
    const auto guard = core::propertyGuard(q(), &Device::state, &Device::stateChanged);

    cancelConnectTimeout();

    if (!serialPort->setBaudRate(speed)) {
        reportError(serialPort->errorString());
        return false;
    }

    if (!serialPort->setParity(QSerialPort::NoParity)) {
        reportError(serialPort->errorString());
        return false;
    }

    if (!serialPort->setDataBits(QSerialPort::Data8)) {
        reportError(serialPort->errorString());
        return false;
    }

    if (!serialPort->setStopBits(QSerialPort::OneStop)) {
        reportError(serialPort->errorString());
        return false;
    }

    if (!serialPort->setFlowControl(QSerialPort::HardwareControl)) {
        reportError(serialPort->errorString());
        return false;
    }


    if (!serialPort->open(QSerialPort::ReadWrite)) {
        reportError(serialPort->errorString());
        return false;
    }

    connectTimeoutId = startTimer(5s);

    connect(serialPort, &QSerialPort::readyRead, this, &Private::onReadyRead);

    streamReader.setDevice(serialPort);
    streamWriter.setDevice(serialPort);

    startCommunication();

    return true;
}

bool Device::connectToDevice()
{
    return d->connectToDevice();
}

void Device::disconnectFromDevice()
{
    const auto guard = core::propertyGuard(this, &Device::state, &Device::stateChanged);

    d->cancelConnectTimeout();
    d->serialPort->disconnect(this);
    d->streamReader.setDevice(nullptr);
    d->streamWriter.setDevice(nullptr);
    d->serialPort->close();
}

QVariant Device::deviceInfo(core::DeviceInfo id, int role) const
{
    if (role == Qt::EditRole || role == Qt::UserRole)
        return d->deviceInfo[id];

    return {};
}

void Device::Private::setDeviceInfo(core::DeviceInfo id, QVariant value)
{
    // FIXME: share setDeviceInfo()
    if (auto it = deviceInfo.find(id); it == deviceInfo.end()) {
        deviceInfo.insert(id, std::move(value));
        emit q()->deviceInfoChanged({id}, {});
    } else if (*it != value) {
        *it = std::move(value);
        emit q()->deviceInfoChanged({id}, {});
    }
}

void Device::setDeviceInfo(core::DeviceInfo id, QVariant value)
{
    d->setDeviceInfo(id, std::move(value));
}

void Device::Private::updateDeviceInfo()
{
    powerControl->updateState();
    queryStationEquipment();
    queryStationStatus();
}

void Device::updateDeviceInfo()
{
    d->updateDeviceInfo();
}

core::DeviceFactory *Device::factory() { return d->factory; }
core::AccessoryControl *Device::accessoryControl() { return d->accessoryControl; }
core::DebugControl *Device::debugControl() { return d->debugControl; }
core::PowerControl *Device::powerControl() { return d->powerControl; }
core::VariableControl *Device::variableControl() { return d->variableControl; }
core::VehicleControl *Device::vehicleControl() { return d->vehicleControl; }

// =====================================================================================================================

QString DeviceFactory::name() const
{
    return tr("ZIMO Commmand Station (MX1, MXUFL, ...)");
}

QList<core::Parameter> DeviceFactory::parameters() const
{
    return {
        core::Parameter::choice<QString>(s_parameter_portName, LMRS_TR("Serial &port:"), defaultPorts()),
        core::Parameter::choice<QSerialPort::BaudRate>(s_parameter_portSpeed, LMRS_TR("&Speed:"), serialSpeeds()),
    };
}

core::Device *DeviceFactory::create(QVariantMap parameters, QObject *parent)
{
    const auto parameterSet = core::ValidatingVariantMap<DeviceFactory>{std::move(parameters)};

    const auto portName = parameterSet.find<QString>(s_parameter_portName);
    const auto portSpeed = parameterSet.find<QSerialPort::BaudRate>(s_parameter_portSpeed);

    if (portName && portSpeed)
        return new Device{portName.value(), portSpeed.value(), this, parent};

    return nullptr;
}

// =====================================================================================================================

} // namespace lmrs::zimo::mx1
