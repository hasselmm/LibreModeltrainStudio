#include "lp2device.h"

#include "lp2request.h"
#include "lp2response.h"
#include "lp2stream.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/parameters.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>
#include <lmrs/core/validatingvariantmap.h>

#include <QEvent>
#include <QSerialPort>
#include <QSerialPortInfo>

namespace lmrs::esu::lp2 {

namespace {

constexpr auto s_parameter_portName = "port"_BV;

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

constexpr auto makeError(Response::Status status)
{
    switch (status) {
    case Response::Status::Success:
        return core::Error::NoError;
    case Response::Status::InvalidRequest:
        return core::Error::RequestFailed;
    case Response::Status::InvalidArgument:
        return core::Error::InvalidRequest;

    case Response::Status::UnknownError:
    case Response::Status::OperationFailed:
    case Response::Status::InvalidSequence:
    case Response::Status::IncompletePacket:
    case Response::Status::InvalidChecksum:
    case Response::Status::Overcurrent:
        break;
    }

    return core::Error::RequestFailed;
}

constexpr std::optional<core::DeviceInfo> toDeviceInfo(InterfaceInfo id)
{
    switch (id) {
    case InterfaceInfo::ManufacturerId:
        return core::DeviceInfo::ManufacturerId;
    case InterfaceInfo::ProductId:
        return core::DeviceInfo::ProductId;
    case InterfaceInfo::SerialNumber:
        return core::DeviceInfo::SerialNumber;
    case InterfaceInfo::ProductionDate:
        return core::DeviceInfo::ProductionDate;
    case InterfaceInfo::BootloaderCode:
        return core::DeviceInfo::BootloaderVersion;
    case InterfaceInfo::BootloaderDate:
        return core::DeviceInfo::BootloaderDate;
    case InterfaceInfo::ApplicationCode:
        return core::DeviceInfo::FirmwareVersion;
    case InterfaceInfo::ApplicationDate:
        return core::DeviceInfo::FirmwareDate;
    case InterfaceInfo::ApplicationType:
        return core::DeviceInfo::FirmwareType;
    }

    return {};
}

} // namespace

// =====================================================================================================================

class Device::DebugControl : public core::DebugControl
{
public:
    explicit DebugControl(Private *d);

    Device *device() const override { return core::checked_cast<Device *>(parent()); }

    Features features() const noexcept override { return Feature::DccFrames | Feature::NativeFrames; }
    QString nativeProtocolName() const noexcept override { return tr("LP2 Protocol"); }

    void sendDccFrame(QByteArray frame, DccPowerMode powerMode, DccFeedbackMode feedbackMode) const override;

private:
    Private *d() const;
};

class Device::PowerControl : public core::PowerControl
{
public:
    explicit PowerControl(Private *d);

    Device *device() const override { return core::checked_cast<Device *>(parent()); }
    State state() const override { return m_state; }

    void enterServiceMode(core::ContinuationCallback<core::Error> callback);
    void enableTrackPower(core::ContinuationCallback<core::Error> callback) override;
    void disableTrackPower(core::ContinuationCallback<core::Error> callback) override;

private:
    Private *d() const;

    State m_state = State::PowerOff;
};

class Device::VariableControl : public core::VariableControl
{
public:
    explicit VariableControl(Private *d);

    Device *device() const override { return core::checked_cast<Device *>(parent()); }
    Features features() const override { return Feature::DirectProgramming; }

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

    Device *device() const override { return core::checked_cast<Device *>(parent()); }

    // NOTE: Can't implement subscriptions, unless we learn how to read Railcom responses on this device.
    void subscribe(dcc::VehicleAddress, SubscriptionType) override {}

    void setSpeed(dcc::VehicleAddress address, dcc::Speed speed, dcc::Direction direction) override;
    void setFunction(dcc::VehicleAddress address, dcc::Function function, bool enabled) override;

protected:
    void timerEvent(QTimerEvent *event) override;

private:
    void onPowerStateChanged(PowerControl::State state);

    Private *d() const;

    int dccRequestTimerId = 0;

    dcc::VehicleAddress currentAddress = 0;

    struct VehicleState {
        dcc::Speed speed;
        dcc::Direction direction = dcc::Direction::Forward;
        dcc::FunctionState functions;
    };

    QHash<dcc::VehicleAddress, VehicleState> vehicles;
};

// =====================================================================================================================

class Device::Private : public core::PrivateObject<Device>
{
public:
    explicit Private(QString portName, DeviceFactory *factory, Device *parent)
        : PrivateObject{parent}
        , serialPort{std::move(portName), this}
        , factory{factory}
    {}

    using ResponseCallback = std::function<void(Response)>;
    void sendRequest(Request request, ResponseCallback callback);

    void reportError(QString message);
    void parseMessage(QByteArray data);

    void onReadyRead();

    void timerEvent(QTimerEvent *event) override;
    void cancelConnectTimeout();

    StreamReader streamReader;
    StreamWriter streamWriter;

    core::ConstPointer<QSerialPort> serialPort;
    core::ConstPointer<DeviceFactory> factory;

    core::ConstPointer<DebugControl> debugControl{this};
    core::ConstPointer<PowerControl> powerControl{this};
    core::ConstPointer<VariableControl> variableControl{this};
    core::ConstPointer<VehicleControl> vehicleControl{this};

    int connectTimeoutId = 0;

    struct PendingRequest
    {
        PendingRequest(Request request, ResponseCallback callback) noexcept
            : request{std::move(request)}
            , callback{std::move(callback)}
        {}

        Request request;
        ResponseCallback callback;
    };

    QHash<core::DeviceInfo, QVariant> deviceInfo;
    QHash<Request::Sequence, PendingRequest> pendingRequests;
};

// =====================================================================================================================

Device::DebugControl::DebugControl(Private *d)
    : core::DebugControl{d->q()}
{}

DccSettings dccSettings(core::DebugControl::DccFeedbackMode feedbackMode)
{
    switch (feedbackMode) {
    case core::DebugControl::DccFeedbackMode::Acknowledge:
        return DccSettings::ack();
    case core::DebugControl::DccFeedbackMode::Advanced:
        return DccSettings::afb();
    case core::DebugControl::DccFeedbackMode::None:
        break;
    }

    return DccSettings::nack();
}

void Device::DebugControl::sendDccFrame(QByteArray frame, DccPowerMode powerMode, DccFeedbackMode feedbackMode) const
{
    const auto dcc = DccRequest{dccSettings(feedbackMode), std::move(frame)};

    auto callback = [this, dcc](core::Error error) {
        if (error != core::Error::NoError)
            return core::Continuation::Retry;

        auto request = Request{Request::Identifier::SendDcc, dcc.toByteArray()};
        static_cast<Device *>(device())->d->sendRequest(std::move(request), [](auto /*response*/) {
        });

        return core::Continuation::Proceed;
    };

    const auto powerControl = static_cast<PowerControl *>(device()->powerControl());

    switch (powerMode) {
    case DccPowerMode::Track:
        powerControl->enableTrackPower(std::move(callback));
        break;

    case DccPowerMode::Service:
        powerControl->enterServiceMode(std::move(callback));
        break;
    }
}

Device::PowerControl::PowerControl(Private *d)
    : core::PowerControl{d->q()}
{}

void Device::PowerControl::enterServiceMode(core::ContinuationCallback<core::Error> callback)
{
    d()->sendRequest(Request::reset(), [this, callback](Response response) {
        qInfo() << "reset:" << response;

        d()->sendRequest(Request::powerOn(PowerSettings::service()), [this, callback](Response response) {
            if (const auto valueResponse = response.get<ValueResponse>(); !valueResponse) {
                qCWarning(logger(this), "entering service mode failed: unexpected response type");
                qCWarning(logger(this)) << response;
                // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
                core::callIfDefined(core::Continuation::Retry, callback, core::Error::RequestFailed);
            } else if (valueResponse->status() != Response::Status::Success) {
                qCWarning(logger(this), "entering service mode failed: 0x%02x", core::value(valueResponse->status()));
                // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
                core::callIfDefined(core::Continuation::Retry, callback, makeError(valueResponse->status()));
            } else {
                d()->sendRequest(Request{Request::Identifier::SetSomeMagic1, "02"_hex}, [this, callback](Response response) {
                    qCInfo(logger(this)) << "magic1 response:" << response;

                    if (!response.isValid()) {
                        // FIXME: actually really check for errors
                        core::callIfDefined(core::Continuation::Retry, callback, core::Error::RequestFailed);
                    } else {
                        d()->sendRequest(Request::setAcknowledgeMode({}), [this, callback](Response response) {
                            if (const auto valueResponse = response.get<ValueResponse>(); !valueResponse) {
                                qCWarning(logger(this), "setting acknowledge mode failed: unexpected response type");
                                qCWarning(logger(this)) << response;
                                // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
                                core::callIfDefined(core::Continuation::Retry, callback, core::Error::RequestFailed);
                            } else if (valueResponse->status() != Response::Status::Success) {
                                qCWarning(logger(this), "setting acknowledge mode failed: 0x%02x", core::value(valueResponse->status()));
                                // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
                                core::callIfDefined(core::Continuation::Retry, callback, makeError(valueResponse->status()));
                            } else {
                                const auto stateGuard = core::propertyGuard(this, &PowerControl::state, &PowerControl::stateChanged);
                                m_state = State::ServiceMode; // FIXME: is there really no way to query current power mode on LP2?
                                // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
                                core::callIfDefined(core::Continuation::Proceed, callback, core::Error::NoError);
                            }
                        });
                    }
                });
            }
        });
    });
}

void Device::PowerControl::enableTrackPower(core::ContinuationCallback<core::Error> callback)
{
    d()->sendRequest(Request::reset(), [this, callback](Response response) {
        qInfo() << "reset:" << response;

        d()->sendRequest(Request::powerOn(PowerSettings::driving()), [this, callback](Response response) {
            if (const auto valueResponse = response.get<ValueResponse>(); !valueResponse) {
                qCWarning(logger(this), "enabling track power failed: unexpected response type");
                qCWarning(logger(this)) << response;
                // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
                core::callIfDefined(core::Continuation::Retry, callback, core::Error::RequestFailed);
            } else if (valueResponse->status() != Response::Status::Success) {
                qCWarning(logger(this), "enabling track power failed: 0x%02x", core::value(valueResponse->status()));
                // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
                core::callIfDefined(core::Continuation::Retry, callback, makeError(valueResponse->status()));
            } else {
                d()->sendRequest(Request{Request::Identifier::SetSomeMagic1, "01"_hex}, [this, callback](Response response) {
                    qCInfo(logger(this)) << "magic1 response:" << response;

                    if (!response.isValid()) {
                        // FIXME: actually really check for errors
                        core::callIfDefined(core::Continuation::Retry, callback, core::Error::RequestFailed);
                    } else {
                        const auto stateGuard = core::propertyGuard(this, &PowerControl::state, &PowerControl::stateChanged);
                        m_state = State::PowerOn; // FIXME: is there really no way to query current power mode on LP2?
                        // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
                        core::callIfDefined(core::Continuation::Proceed, callback, core::Error::NoError);
                    }
                });
            }
        });
    });
}

void Device::PowerControl::disableTrackPower(core::ContinuationCallback<core::Error> callback)
{
// FIXME: handle callback, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
    d()->sendRequest(Request::powerOff(), [this, callback](Response response) {
        if (const auto valueResponse = response.get<ValueResponse>(); !valueResponse) {
            qCWarning(logger(this), "disabling track power: unexpected response type");
            qCWarning(logger(this)) << response;
            // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
            core::callIfDefined(core::Continuation::Retry, callback, core::Error::RequestFailed);
        } else if (valueResponse->status() != Response::Status::Success) {
            qCWarning(logger(this), "disabling track power failed: 0x%02x", core::value(valueResponse->status()));
            // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
            core::callIfDefined(core::Continuation::Retry, callback, makeError(valueResponse->status()));
        } else {
            const auto stateGuard = core::propertyGuard(this, &PowerControl::state, &PowerControl::stateChanged);
            m_state = State::PowerOff; // FIXME: is there really no way to query current power mode on LP2?
            // FIXME: handle continuation result, but first find a better pattern than the 16 lines approach in z21::PowerControl::enablePower()
            core::callIfDefined(core::Continuation::Proceed, callback, core::Error::NoError);
        }
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

void Device::VariableControl::readVariable(dcc::VehicleAddress address, dcc::VariableIndex variable,
                                           core::ContinuationCallback<VariableValueResult> callback)
{
    if (address != 0) {
        qCWarning(core::logger<Device>(), "POM mode is not supported for ESU LokProgrammer");
        core::callIfDefined(core::Continuation::Abort, callback, {core::Error::InvalidRequest, {}});
        return;
    }

    // FIXME: also work with track power enabled, and maybe also restore track power

    d()->powerControl->enterServiceMode([this, variable, callback](core::Error error) {
        if (error != core::Error::NoError)
            return core::Continuation::Retry;

        struct Closure
        {
            VariableControl *const control;

            decltype(variable) variable;
            decltype(callback) callback;

            enum State { Reset, VerifyBit, VerifyByte } state = Reset;

            quint8 currentBit = 0;
            quint8 currentValue = 0;

            void start()
            {
                currentBit = 0;
                currentValue = 0;
                proceed(Reset);
            }

            void proceed(State newState)
            {
                switch (state = newState) {
                case Reset:
                    control->d()->sendRequest(Request::sendDcc(DccRequest::reset(5)), *this);
                    break;

                case VerifyBit:
                    control->d()->sendRequest(Request::sendDcc(DccRequest::verifyBit(variable, false, currentBit)), *this);
                    break;

                case VerifyByte:
                    control->d()->sendRequest(Request::sendDcc(DccRequest::verifyByte(variable, currentValue)), *this);
                    break;
                }
            }

            void operator()(Response response)
            {
                switch (state) {
                case Reset:
                    if (const auto dcc = response.get<DccResponse>(); !dcc
                            || dcc->status() != Response::Status::Success) {
                        qCWarning(logger(control), "Bad response to reset request");
                        reportResult(core::Error::RequestFailed);
                    } else {
                        proceed(VerifyBit);
                    }

                    return;

                case VerifyBit:
                    if (const auto dcc = response.get<DccResponse>(); !dcc
                            || dcc->status() != Response::Status::Success) {
                        qCWarning(logger(control), "Bad response to verify bit request");
                        reportResult(core::Error::RequestFailed);
                    } else {
                        if (dcc->acknowledge() == DccResponse::Acknowledge::Negative)
                            currentValue |= static_cast<quint8>(1 << currentBit);

                        proceed(++currentBit < 8 ? Reset : VerifyByte);
                    }

                    return;

                case VerifyByte:
                    if (const auto dcc = response.get<DccResponse>(); !dcc
                            || dcc->status() != Response::Status::Success
                            || dcc->acknowledge() != DccResponse::Acknowledge::Positive) {
                        qCWarning(logger(control), "Bad response to verify byte request");
                        reportResult(core::Error::RequestFailed);
                    } else {
                        control->d()->powerControl->disableTrackPower({});
                        reportResult(core::Error::NoError);
                    }

                    return;
                }
            }

            void reportResult(core::Error error)
            {
                switch (core::callIfDefined(core::Continuation::Proceed, callback, {error, currentValue})) {
                case core::Continuation::Retry:
                    if (callback = callback.retry(); callback)
                        proceed(state == VerifyBit ? Reset : state);

                    break;

                case core::Continuation::Proceed:
                case core::Continuation::Abort:
                    break;
                }
            }
        };

        Closure{this, variable, callback}.start();
        return core::Continuation::Proceed;
    });
}

void Device::VariableControl::writeVariable(dcc::VehicleAddress /*address*/, dcc::VariableIndex /*variable*/,
                                            dcc::VariableValue /*value*/, core::ContinuationCallback<VariableValueResult> /*callback*/)
{
    LMRS_UNIMPLEMENTED();
}

Device::Private *Device::VariableControl::d() const
{
    return core::checked_cast<Device *>(parent())->d;
}

// =====================================================================================================================

Device::VehicleControl::VehicleControl(Private *d)
    : core::VehicleControl{d->q()}
{
    connect(d->powerControl, &PowerControl::stateChanged, this, &VehicleControl::onPowerStateChanged);
}

void Device::VehicleControl::setSpeed(dcc::VehicleAddress address, dcc::Speed speed, dcc::Direction direction)
{
    currentAddress = address;
    vehicles[address].speed = speed;
    vehicles[address].direction = direction;
}

void Device::VehicleControl::setFunction(dcc::VehicleAddress address, dcc::Function function, bool enabled)
{
    currentAddress = address;
    vehicles[address].functions[value(function)] = enabled;
}

void Device::VehicleControl::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == dccRequestTimerId) {
        if (currentAddress != 0) {
            const auto current = vehicles.value(currentAddress);

            if (std::holds_alternative<dcc::Speed14>(current.speed)) {
                const auto speed = std::get<dcc::Speed14>(current.speed);
                auto request = DccRequest::setSpeed14(currentAddress, speed.count(), current.direction, current.functions[0]);
                d()->sendRequest(Request::sendDcc(std::move(request)), {}); // FIXME callback?
            } else if (std::holds_alternative<dcc::Speed28>(current.speed)) {
                const auto speed = std::get<dcc::Speed28>(current.speed);
                auto request = DccRequest::setSpeed28(currentAddress, speed.count(), current.direction);
                d()->sendRequest(Request::sendDcc(std::move(request)), {}); // FIXME callback?
            } else {
                const auto speed = speedCast<dcc::Speed126>(current.speed);
                auto request = DccRequest::setSpeed126(currentAddress, speed.count(), current.direction);
                d()->sendRequest(Request::sendDcc(std::move(request)), {}); // FIXME callback?
            }

            for (auto groupId: QMetaTypeId<dcc::FunctionGroup>()) {
                if (groupId.value() == dcc::FunctionGroup::None)
                    continue;

                const auto mask = functionMask(dcc::functionRange(groupId.value()), current.functions);
                auto request = DccRequest::setFunctions(currentAddress, groupId.value(), mask);
                d()->sendRequest(Request::sendDcc(std::move(request)), {}); // FIXME callback?
            }
        }
    }
}

void Device::VehicleControl::onPowerStateChanged(PowerControl::State state)
{
    if (state == PowerControl::State::PowerOn) {
        if (dccRequestTimerId == 0) {
            qCInfo(logger(this), "starting DCC request timer");
            dccRequestTimerId = startTimer(100ms);
        }
    } else {
        if (const auto timerId = std::exchange(dccRequestTimerId, 0); timerId != 0) {
            qCInfo(logger(this), "stopping DCC request timer");
            killTimer(timerId);
        }
    }
}

Device::Private *Device::VehicleControl::d() const
{
    return core::checked_cast<Device *>(parent())->d;
}

// =====================================================================================================================

void Device::Private::sendRequest(Request request, std::function<void (Response)> callback)
{
    const auto guard = core::propertyGuard(q(), &Device::state, &Device::stateChanged);

    if (!serialPort->isOpen()) {
        reportError(tr("Cannot send to disconnected device"));
        core::callIfDefined(callback, Response{});
        return;
    }

    auto data = request.toByteArray();
    qCDebug(logger(), ">W %s", data.toHex(' ').constData());
    // qCDebug(lcProgrammer) << ">>" << request;

    if (!streamWriter.writeFrame(std::move(data))) {
        reportError(streamWriter.errorString());
        core::callIfDefined(callback, Response{});
        return;
    }

    const auto sequence = request.sequence(); // store before moving away the request
    pendingRequests.insert(sequence, {std::move(request), std::move(callback)});
}

void Device::Private::reportError(QString message)
{
    qCWarning(logger(), "Error occured: %ls", qUtf16Printable(message));
    q()->disconnectFromDevice();
}

void Device::Private::parseMessage(QByteArray data)
{
    if (auto message = Message{std::move(data)}; message.isValid()) {
        if (const auto it = pendingRequests.find(message.sequence()); it != pendingRequests.end()) {
            if (auto response = Response{it->request, std::move(message)}; response.isValid()) {
//                    qCDebug(lcProgrammer) << "<<" << response;
                const auto callback = std::move(it->callback);
                pendingRequests.erase(it);

                core::callIfDefined(callback, std::move(response));
            }
        } else {
            qCWarning(logger(), "Unexpected response for request #%d (0x%02x)",
                      message.sequence(), message.sequence());
            qInfo() << pendingRequests.keys();
        }
    } else {
        qCWarning(logger(), "Malformed message");
    }
}

void Device::Private::onReadyRead()
{
    // FIXME: do not expect start and end marker in message
    while (streamReader.readNext()) {
        auto frame = streamReader.frame();
        qCDebug(logger(), "<R %s", frame.toHex(' ').constData());
        parseMessage(std::move(frame));
    }
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

// =====================================================================================================================

Device::Device(QString portName, DeviceFactory *factory, QObject *parent)
    : core::Device{parent}
    , d{std::move(portName), factory, this}
{
    qRegisterMetaType<Device>();

    qRegisterMetaType<InterfaceApplicationType>();
    qRegisterMetaType<InterfaceApplicationTypes>();
    qRegisterMetaType<InterfaceFlag>();
    qRegisterMetaType<InterfaceFlags>();

    observe(core::DeviceInfo::TrackStatus,
            static_cast<core::PowerControl *>(d->powerControl.get()),
            &core::PowerControl::state, &core::PowerControl::stateChanged);

    connect(d->serialPort, &QSerialPort::readyRead, d, &Private::onReadyRead);
}

core::Device::State Device::state() const
{
    if (!d->serialPort->isOpen())
        return State::Disconnected;
    if (d->deviceInfo.isEmpty())
        return State::Connecting;

    return State::Connected;
}

QString Device::name() const
{
    return tr("LokProgrammer at %1").arg(d->serialPort->portName());
}

QString Device::uniqueId() const
{
    // FIXME: how do we read the devices' serial number?
    return "esu:lp2:"_L1 + d->serialPort->portName();
}

bool Device::connectToDevice()
{
    const auto guard = core::propertyGuard(this, &Device::state, &Device::stateChanged);

    d->cancelConnectTimeout();

    if (!d->serialPort->setBaudRate(QSerialPort::Baud115200)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    if (!d->serialPort->setStopBits(QSerialPort::OneStop)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    if (!d->serialPort->setParity(QSerialPort::NoParity)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    if (!d->serialPort->setDataBits(QSerialPort::Data8)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    if (!d->serialPort->setFlowControl(QSerialPort::HardwareControl)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }


    if (!d->serialPort->open(QSerialPort::ReadWrite)) {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    if (!d->serialPort->setDataTerminalReady(false))  {
        d->reportError(d->serialPort->errorString());
        return false;
    }

    d->connectTimeoutId = d->startTimer(5s);

    d->streamReader.setDevice(d->serialPort);
    d->streamWriter.setDevice(d->serialPort);

    updateDeviceInfo();

    return true;
}

void Device::disconnectFromDevice()
{
    const auto guard = core::propertyGuard(this, &Device::state, &Device::stateChanged);

    d->cancelConnectTimeout();
    d->streamReader.setDevice(nullptr);
    d->streamWriter.setDevice(nullptr);
    d->serialPort->close();
}

core::DeviceFactory *Device::factory() { return d->factory; }
core::DebugControl *Device::debugControl() { return d->debugControl; }
core::PowerControl *Device::powerControl() { return d->powerControl; }
core::VariableControl *Device::variableControl() { return d->variableControl; }
core::VehicleControl *Device::vehicleControl() { return d->vehicleControl; }

QVariant Device::deviceInfo(core::DeviceInfo id, int role) const
{
    if (role == Qt::EditRole || role == Qt::UserRole)
        return d->deviceInfo[id];

    if (role == Qt::DisplayRole && id == core::DeviceInfo::ProductId) {
        const auto productId = d->deviceInfo.value(id).toUInt();
        const auto productFamily = (productId >> 24);

        if (productFamily == 1) {
            if (productId <= 0x100000c)
                return tr("LokProgrammer 3.3");
            if (productId <= 0x1000057)
                return tr("LokProgrammer 3.6");

            return tr("LokProgrammer");
        } else if (productFamily == 2) {
            if (productId <= 0x20000ca)
                return tr("ProfiTester");
        }
    }

    return {};
}

void Device::updateDeviceInfo()
{
    QList<InterfaceInfo> deviceInfoIds;

    for (const auto &entry: QMetaTypeId<InterfaceInfo>())
        deviceInfoIds.append(entry.value());

    readDeviceInformation(deviceInfoIds, [this](auto result, auto values) {
        const auto guard = core::propertyGuard(this, &Device::state, &Device::stateChanged);

        d->cancelConnectTimeout();

        if (result != Result::Success
                || values[InterfaceInfo::ManufacturerId].isNull()
                || values[InterfaceInfo::ProductId].isNull()) {
            d->reportError(tr("Could not identify hardware"));
            return;
        }

        // convert LP2 interface info into generic device info
        for (auto it = values.begin(); it != values.end(); ++it) {
            if (const auto id = toDeviceInfo(it.key()))
                d->deviceInfo.insert(id.value(), it.value());
            else
                qCWarning(logger(this)) << "Unsupported interface info:" << it.key();
        }

        emit deviceInfoChanged(d->deviceInfo.keys());
    });
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

void Device::readDeviceInformation(QList<InterfaceInfo> ids, ReadDeviceInfoCallback callback)
{
    struct Context
    {
        using IdList = QList<InterfaceInfo>;
        using Callback = decltype(callback);
        using ValueHash = QHash<InterfaceInfo, QVariant>;

        Context(IdList ids, Callback callback, ValueHash values = {})
            : ids{std::move(ids)}
            , callback{std::move(callback)}
            , values{std::move(values)}
        {}

        const IdList ids;
        const Callback callback;
        ValueHash values;
    };

    const auto context = std::make_shared<Context>(std::move(ids), std::move(callback));
    d->sendRequest(Request::reset(), [this, context](Response response) {
        qInfo() << "reset:" << response; // FIXME handle response

        if (!response.isValid()) {
            core::callIfDefined(context->callback, Result::Failure, {});
            return;
        }

        for (const auto id: context->ids) {
            d->sendRequest(Request::interfaceInfo(id), [this, id, context](Response response) {
                if (const auto infoResponse = response.get<InterfaceInfoResponse>();
                        infoResponse && infoResponse->status() == Response::Status::Success) {
                    context->values.insert(id, infoResponse->value());
                } else {
                    qCWarning(logger(this), "getting device information %s failed: 0x%02x",
                              QMetaEnum::fromType<decltype(id)>().valueToKey(static_cast<int>(id)),
                              static_cast<int>(infoResponse->status()));

                    context->values.insert(id, {});
                }

                if (context->ids.size() == context->values.size() && context->callback)
                    core::callIfDefined(context->callback, Result::Success, std::move(context->values));
            });
        }
    });

}

// =====================================================================================================================

QString DeviceFactory::name() const
{
    return tr("ESU LokProgrammer");
}

QList<core::Parameter> DeviceFactory::parameters() const
{
    return {
        core::Parameter::choice<QString>(s_parameter_portName, LMRS_TR("Serial &port:"), defaultPorts()),
    };
}

core::Device *DeviceFactory::create(QVariantMap parameters, QObject *parent)
{
    const auto parameterSet = core::ValidatingVariantMap<DeviceFactory>{std::move(parameters)};

    if (const auto portName = parameterSet.find<QString>(s_parameter_portName))
        return new Device{portName.value(), this, parent};

    return nullptr;
}

// =====================================================================================================================

} // namespace lmrs::esu::lp2
