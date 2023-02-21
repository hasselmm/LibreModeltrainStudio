#include "lokprogrammer.h"

#include <lmrs/core/algorithms.h>
#include <lmrs/core/propertyguard.h>
#include <lmrs/core/typetraits.h>
#include <lmrs/core/userliterals.h>

#include <lmrs/esu/lp2request.h>
#include <lmrs/esu/lp2response.h>
#include <lmrs/esu/lp2stream.h>

#include <QHash>
#include <QLoggingCategory>
#include <QMetaEnum>
#include <QPointer>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QVariant>

using namespace lmrs::esu::lp2; // FIXME
using namespace lmrs;

namespace esu::programmer {

namespace {

using lmrs::core::propertyGuard;

Q_LOGGING_CATEGORY(lcProgrammer, "programmer")

constexpr auto s_vendorFtdi = 1027;

template <typename... Args>
void callIfDefined(std::function<void(Args...)> callable, Args... args)
{
    if (callable)
        callable(args...);
}

} // namespace

class LokProgrammer::Private : public core::PrivateObject<LokProgrammer>
{
public:
    using PrivateObject::PrivateObject;

    QString errorString;
    StreamReader streamReader;
    StreamWriter streamWriter;
    QPointer<QSerialPort> serialPort;

    struct PendingRequest {
        Request request;
        ResponseCallback callback;
    };

    QHash<Request::Sequence, PendingRequest> pendingRequests;
    PowerSettings::Mode powerMode = PowerSettings::Disabled; // HACK

    void reportError(QString errorString);
    void parseMessage(QByteArray data);

    void sendRequest(Request request, ResponseCallback callback);

    void onReadyRead();
};

LokProgrammer::LokProgrammer(QObject *parent)
    : QObject{parent}
    , d{new Private{this}}
{}

QStringList LokProgrammer::availablePorts()
{
    QStringList availablePorts;

    for (const auto &info: QSerialPortInfo::availablePorts()) {
        if (info.vendorIdentifier() == s_vendorFtdi)
            availablePorts.append(info.portName());
    }

    return availablePorts;
}

QString LokProgrammer::errorString() const
{
    return d->errorString;
}

LokProgrammer::State LokProgrammer::state() const
{
    if (d->serialPort.isNull())
        return State::Disconnected;
    if (!d->errorString.isEmpty())
        return State::Error;
    if (!d->serialPort->isOpen())
        return State::Connecting;

    return State::Connected;
}

void LokProgrammer::powerOn(PowerSettings setting, ResultCallback callback)
{
    qInfo() << "requested" << setting << "actual" << d->powerMode;
    // FIXME: we also should check the other attributes of PowerMode
    if (d->powerMode == setting.mode()) {
        callIfDefined(callback, Result::Success);
        return;
    }

    sendRequest(Request::reset(), [this, setting, callback](Response response) {
        qInfo() << "reset:" << response;
        qInfo() << "powerMode:" << setting;

        sendRequest(Request::powerOn(setting), [this, setting, callback](Response response) {
            switch (setting.mode()) {
            case PowerSettings::Disabled:
                if (const auto valueResponse = response.get<ValueResponse>(); !valueResponse) {
                    qCWarning(lcProgrammer, "disabling power: unexpected response type");
                    qWarning() << response;
                    callIfDefined(callback, Result::Failure);
                } else if (valueResponse->status() != Response::Status::Success) {
                    qCWarning(lcProgrammer, "disabling power failed: 0x%02x",
                              static_cast<int>(valueResponse->status()));
                    callIfDefined(callback, Result::Failure);
                } else {
                    d->powerMode = PowerSettings::Disabled; // HACK
                    callIfDefined(callback, Result::Success);
                }

                return;

            case PowerSettings::Enabled:
                if (const auto valueResponse = response.get<ValueResponse>(); !valueResponse) {
                    qCWarning(lcProgrammer, "enabling power failed: unexpected response type");
                    qWarning() << response;
                    callIfDefined(callback, Result::Failure);
                } else if (valueResponse->status() != Response::Status::Success) {
                    qCWarning(lcProgrammer, "enabling power failed: 0x%02x",
                              static_cast<int>(valueResponse->status()));
                    callIfDefined(callback, Result::Failure);
                } else {
                    sendRequest(Request{Request::Identifier::SetSomeMagic1, "01"_hex}, [this, callback](Response response) {
                        qInfo() << "magic1 response:" << response;

    //                    top.mainwindow: >W 22 Request (Identifier::SetPowerMode, "01 00 2d 19")
    //                    top.mainwindow: <R 22 Response (Identifier::DeviceFlagsResponse, QVariant(esu::programmer::DeviceFlagsResponse, (Status::Success)))
    //                    top.mainwindow: >W 23 Request (Identifier::SetSomeMagic1, "01")
    //                    top.mainwindow: <R 23 Response (Identifier::DeviceFlagsResponse, QVariant(esu::programmer::DeviceFlagsResponse, (Status::Success)))

                        d->powerMode = PowerSettings::Enabled; // HACK
                        callIfDefined(callback, Result::Success);
                    });
                }

                return;

            case PowerSettings::Service:
                if (const auto valueResponse = response.get<ValueResponse>(); !valueResponse) {
                    qCWarning(lcProgrammer, "enabling power failed: unexpected response type");
                    qInfo() << response;
                    callIfDefined(callback, Result::Failure);
                } else if (valueResponse->status() != Response::Status::Success) {
                    qCWarning(lcProgrammer, "enabling power failed: 0x%02x",
                              static_cast<int>(valueResponse->status()));
                    callIfDefined(callback, Result::Failure);
                } else {
                    sendRequest(Request{Request::Identifier::SetSomeMagic1, "02"_hex}, {});
                    sendRequest(Request::setAcknowledgeMode({}), [this, callback](Response response) {
                        if (const auto valueResponse = response.get<ValueResponse>(); !valueResponse) {
                            qCWarning(lcProgrammer, "setting NMRA power failed: unexpected response type");
                            qInfo() << response;
                            callIfDefined(callback, Result::Failure);
                        } else if (valueResponse->status() != Response::Status::Success) {
                            qCWarning(lcProgrammer, "setting NMRA power failed: 0x%02x",
                                      static_cast<int>(valueResponse->status()));
                            callIfDefined(callback, Result::Failure);
                        } else {
                            d->powerMode = PowerSettings::Service; // HACK
                            callIfDefined(callback, Result::Success);
                        }
                    });
                }

                return;
            }

            qCWarning(lcProgrammer, "enabling power failed: unsupported power mode");
        });
    });
}

void LokProgrammer::powerOff(ResultCallback callback)
{
    d->sendRequest(Request::powerOff(), [this, callback](Response response) {
        if (const auto valueResponse = response.get<ValueResponse>(); !valueResponse) {
            qCWarning(lcProgrammer, "disabling power: unexpected response type");
            callIfDefined(callback, Result::Failure);
        } else if (valueResponse->status() != Response::Status::Success) {
            qCWarning(lcProgrammer, "disabling power failed: 0x%02x",
                      static_cast<int>(valueResponse->status()));
            callIfDefined(callback, Result::Failure);
        } else {
            d->powerMode = PowerSettings::Disabled; // HACK
            callIfDefined(callback, Result::Success);
        }
    });
}

void LokProgrammer::sendRequest(Request request, ResponseCallback callback)
{
    d->sendRequest(std::move(request), std::move(callback));
}

void LokProgrammer::readDeviceInformation(QList<InterfaceInfo> ids, ReadDeviceInfoCallback callback)
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

        const QList<InterfaceInfo> ids;
        const decltype(callback) callback;
        QHash<InterfaceInfo, QVariant> values;
    };

    const auto context = std::make_shared<Context>(std::move(ids), std::move(callback));
    d->sendRequest(Request::reset(), [this, context](Response response) {
        qInfo() << "reset:" << response;

        for (const auto id: context->ids) {
            d->sendRequest(Request::interfaceInfo(id), [id, context](Response response) {
                if (const auto infoResponse = response.get<InterfaceInfoResponse>();
                        infoResponse && infoResponse->status() == Response::Status::Success) {
                    context->values.insert(id, infoResponse->value());
                } else {
                    qCWarning(lcProgrammer, "getting device information %s failed: 0x%02x",
                              core::key(id), static_cast<int>(infoResponse->status()));
                    context->values.insert(id, {});
                }

                if (context->ids.size() == context->values.size() && context->callback)
                    context->callback(std::move(context->values));
            });
        }
    });
}

void LokProgrammer::readVariable(dcc::VariableIndex variable, ReadVariableCallback callback)
{
    if (outOfRange<dcc::VariableIndex>(variable)) {
        qCWarning(lcProgrammer, "Variable out of range (1..1024)");
        callIfDefined(std::move(callback), Result::Failure, {});
        return;
    }

    struct Context
    {
        using Callback = decltype(callback);

        Context(Callback callback, quint16 variable)
            : callback{std::move(callback)}
            , variable{variable}
        {}

        decltype(callback) callback;
        quint16 variable;
        quint8 value = 0;
        quint8 receivedBits = 0;
        Result result = Result::Success;
    };

    const auto context = std::make_shared<Context>(std::move(callback), variable);

    // FIXME: do not enter programming mode if already in programming mode
    powerOn(PowerSettings{PowerSettings::Service}, [this, context](auto result) {
        if (result != Result::Success) {
            callIfDefined(context->callback, result, {});
            return;
        }

        for (auto i = 0_u8; i < 8_u8; ++i) {
            d->sendRequest(Request::sendDcc(DccRequest::reset(5)), {});
            auto request = Request::sendDcc(DccRequest::verifyBit(context->variable, false, i));
            d->sendRequest(std::move(request), [this, context, bitMask = static_cast<quint8>(1 << i)](Response response) {
                if (const auto dccResponse = response.get<DccResponse>(); !dccResponse
                        || dccResponse->status() != Response::Status::Success) {
                    qCWarning(lcProgrammer, "Bad response to verify bit request");
                    context->result = Result::Failure;
                    return;
                } else {
                    if (dccResponse->acknowledge() == DccResponse::Acknowledge::Negative)
                        context->value |= bitMask;
                }

                // check if all reads have finished
                context->receivedBits |= bitMask;
                if (context->receivedBits != 255)
                    return;

                // if all reads have finished, check they all succeeded
                if (context->result != Result::Success) {
                    callIfDefined(context->callback, context->result, {});
                    return;
                }

                // verify if we got all acknowledges right
                auto request = Request::sendDcc(DccRequest::verifyByte(context->variable, context->value));
                d->sendRequest(std::move(request), [context](Response response) {
                    if (const auto dccResponse = response.get<DccResponse>(); !dccResponse
                            || dccResponse->status() != Response::Status::Success
                            || dccResponse->acknowledge() != DccResponse::Acknowledge::Positive) {
                        qCWarning(lcProgrammer, "Bad response to verify byte request");
                        callIfDefined(context->callback, Result::Failure, context->value);
                        return;
                    }

//                    if (d->pendingRequests.isEmpty()) { // FIXME: do this more globally for all requests
//                        d->sendRequest(Request::powerOff(), {});
//                        d->powerMode = PowerMode::Disabled; // HACK
//                    }

                    callIfDefined(context->callback, Result::Success, context->value);
                });
            });
        }
    });
}

void LokProgrammer::readVariable(dcc::ExtendedVariableIndex variable, ReadVariableCallback callback)
{
    const auto variableIndex = dcc::variableIndex(variable);

    if (inRange<dcc::VariableIndex>(value(variable))) {
        readVariable(variableIndex, std::move(callback));
    } else if (range(dcc::VariableSpace::Extended).contains(variableIndex)) {
        qWarning() << "TODO: extend variable:"
                   << variableIndex << dcc::extendedPage(variable)
                   << dcc::cv31(variable) << dcc::cv32(variable);
        callIfDefined(callback, Result::Failure, {});
    } else if (range(dcc::VariableSpace::Susi).contains(variableIndex)) {
        qWarning() << "TODO: SUSI variable:"
                   << variableIndex << dcc::susiPage(variable);
        callIfDefined(callback, Result::Failure, {});
    } else {
        qCWarning(lcProgrammer, "No paging available for CV%d", variableIndex.value);
        callIfDefined(callback, Result::Failure, {});
    }
}

void LokProgrammer::readVariable(dcc::VehicleVariable variable, ReadVariableCallback callback)
{
    readVariable(dcc::extendedVariable(variable), std::move(callback));
}

void LokProgrammer::writeVariable(dcc::VariableIndex variable, dcc::VariableValue value, WriteVariableCallback callback)
{
    if (outOfRange<dcc::VariableIndex>(variable)) {
        qCWarning(lcProgrammer, "Variable out of range (1..1024)");
        callIfDefined(callback, Result::Failure);
        return;
    }

    powerOn(PowerSettings{PowerSettings::Service}, [this, callback, variable, value](auto result) {
        if (result != Result::Success) {
            callIfDefined(callback, result);
            return;
        }

        d->sendRequest(Request::sendDcc(DccRequest::reset(5)), {});
        auto request = Request::sendDcc(DccRequest::writeByte(variable, value));
        d->sendRequest(std::move(request), [this, callback, variable, value](Response response) {
            if (const auto dccResponse = response.get<DccResponse>(); !dccResponse
                    || dccResponse->status() != Response::Status::Success
                    || dccResponse->acknowledge() != DccResponse::Acknowledge::Positive) {
                qCWarning(lcProgrammer, "Bad response to write byte request");
                callIfDefined(callback, Result::Failure);
                return;
            }

            // verify the byte has been written correctly
            auto request = Request::sendDcc(DccRequest::verifyByte(variable, value));
            d->sendRequest(std::move(request), [this, callback](Response response) {
                if (const auto dccResponse = response.get<DccResponse>(); dccResponse
                        && dccResponse->status() == Response::Status::Success
                        && dccResponse->acknowledge() == DccResponse::Acknowledge::Positive) {
                    if (d->pendingRequests.isEmpty()) { // FIXME: do this more globally for all requests
                        d->sendRequest(Request::powerOff(), {});
                        d->powerMode = PowerSettings::Disabled; // HACK
                    }

                    callIfDefined(callback, Result::Success);
                } else {
                    qCWarning(lcProgrammer, "Bad response to verify byte request");
                    callIfDefined(callback, Result::Failure);
                }
            });
        });
    });
}

bool LokProgrammer::connectToDevice(QString portName)
{
    const auto guard = propertyGuard(this, &LokProgrammer::state, &LokProgrammer::stateChanged);

    if (state() != State::Disconnected) {
        d->reportError(tr("Cannot connect if already connected"));
        return false;
    }

    // just to be sure, actually this should be a nop
    disconnectFromDevice();

    d->serialPort = new QSerialPort{std::move(portName), this};

    emit stateChanged(state()); // we are connected now

    connect(d->serialPort, &QSerialPort::readyRead, d, &Private::onReadyRead);

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

    d->streamReader.setDevice(d->serialPort);
    d->streamWriter.setDevice(d->serialPort);

    return true;
}

void LokProgrammer::disconnectFromDevice()
{
    const auto guard = propertyGuard(this, &LokProgrammer::state, &LokProgrammer::stateChanged);

    if (d->serialPort) {
        d->serialPort->close();
        d->serialPort->disconnect(this);
        d->serialPort->deleteLater();
        d->serialPort.clear();
    }

    d->streamReader.setDevice(nullptr);
    d->streamWriter.setDevice(nullptr);
}

void LokProgrammer::Private::reportError(QString newErrorString)
{
    errorString = std::move(newErrorString);

    emit q()->errorOccured(errorString);
    emit q()->stateChanged(q()->state());
}

void LokProgrammer::Private::sendRequest(Request request, ResponseCallback callback)
{
    const auto guard = propertyGuard(q(), &LokProgrammer::state, &LokProgrammer::stateChanged);

    if (!serialPort) {
        reportError(tr("Cannot send request without device"));
        callIfDefined(callback, {});
        return;
    }

    auto data = request.toByteArray();
    qCDebug(lcProgrammer, ">W %s", data.toHex(' ').constData());
    // qCDebug(lcProgrammer) << ">>" << request;

    if (!streamWriter.writeFrame(std::move(data))) {
        reportError(streamWriter.errorString());
        callIfDefined(callback, {});
        return;
    }

    const auto sequence = request.sequence(); // store before moving away the request
    pendingRequests.insert(sequence, {std::move(request), std::move(callback)});
}

void LokProgrammer::Private::parseMessage(QByteArray data)
{
    if (auto message = Message{std::move(data)}; message.isValid()) {
        if (const auto it = pendingRequests.find(message.sequence()); it != pendingRequests.end()) {
            if (auto response = Response{it->request, std::move(message)}; response.isValid()) {
//                    qCDebug(lcProgrammer) << "<<" << response;
                const auto callback = std::move(it->callback);
                pendingRequests.erase(it);

                callIfDefined(callback, std::move(response));
            }
        } else {
            qCWarning(lcProgrammer, "Unexpected response for request #%d (0x%02x)",
                      message.sequence(), message.sequence());
            qInfo() << pendingRequests.keys();
        }
    } else {
        qCWarning(lcProgrammer, "Malformed message");
    }
}

/*
void LokProgrammer::Private::enterProgrammingMode(ResultCallback callback)
{
    // HACK
    if (powerMode == PowerMode::Programming) {
        callIfDefined(callback, Result::Success);
        return;
    }

    powerMode = PowerMode::Programming; // HACK: do this more properly: by parsing responses, by checking the pending request queue

    sendRequest(Request::reset(), [this, callback](Response response) {
        qInfo() << "reset:" << response;

        sendRequest(Request::powerOn({PowerMode::Programming, 40}), [this, callback](Response response) {
            if (const auto valueResponse = response.get<ValueResponse>();
                    !valueResponse || valueResponse->status() != Response::Status::Success) {
                qCWarning(lcProgrammer, "enabling power failed: 0x%02x",
                          static_cast<int>(valueResponse->status()));
                callIfDefined(callback, Result::Failure);
                return;
            }

            sendRequest(Request::setNmraPowerMode({}), [this, callback](Response response) {
                if (const auto valueResponse = response.get<ValueResponse>();
                        !valueResponse || valueResponse->status() != Response::Status::Success) {
                    qCWarning(lcProgrammer, "setting NMRA power failed: 0x%02x",
                              static_cast<int>(valueResponse->status()));
                    callIfDefined(callback, Result::Failure);
                    return;
                }

                callIfDefined(callback, Result::Success);
            });
        });
    });
}
*/

void LokProgrammer::Private::onReadyRead()
{
    // FIXME: do not expect start and end marker in message
    while (streamReader.readNext()) {
        auto data = streamReader.frame();
        qCDebug(lcProgrammer, "<R %s", data.toHex(' ').constData());
        parseMessage(std::move(data));
    }
}

} // namespace esu::programmer
