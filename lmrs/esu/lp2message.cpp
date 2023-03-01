#include "lp2message.h"

#include "lp2request.h"
#include "lp2response.h"

#include <lmrs/core/logging.h>

#include <QLoggingCategory>
#include <QVariant>

namespace lmrs::esu::lp2 {

// =====================================================================================================================

namespace {

QByteArray makeMessageData(Message::Type type, Message::Sequence sequence,
                           Message::Identifier identifier, QByteArray data)
{
    auto message = QByteArray{Message::HeaderSize + data.size(), Qt::Uninitialized};

    message[0] = static_cast<char>(type);
    message[1] = static_cast<char>(sequence);
    message[2] = static_cast<char>(identifier);

    std::copy(data.begin(), data.end(), message.begin() + Message::HeaderSize);

    return message;
}

} // namespace

// =====================================================================================================================

Message::Message(QByteArray data)
    : m_data{std::move(data)}
{}

Message::Message(Type type, Sequence sequence, Identifier identifier, QByteArray data)
    : Message{makeMessageData(type, sequence, identifier, std::move(data))}
{}

bool Message::isValid() const
{
    return m_data.size() >= HeaderSize;
}

Message::Type Message::type() const
{
    if (Q_LIKELY(isValid()))
        return static_cast<Type>(m_data[0]);

    return Type::Invalid;
}

Message::Sequence Message::sequence() const
{
    if (Q_LIKELY(isValid()))
        return static_cast<Sequence>(m_data[1]);

    return {};
}

Message::Identifier Message::identifier() const
{
    if (Q_LIKELY(isValid()))
        return static_cast<Identifier>(m_data[2]);

    return {};
}

QByteArrayView Message::data() const
{
    return {m_data.begin() + HeaderSize, m_data.end()};
}

int Message::dataSize() const
{
    return static_cast<int>(m_data.size() - HeaderSize);
}

QByteArray Message::toByteArray() const
{
    return m_data;
}

// =====================================================================================================================

Request::Request(Identifier identifier, QByteArray data, Sequence sequence)
    : Message{Type::Request, sequence, static_cast<Message::Identifier>(identifier), std::move(data)}
{}

Message::Sequence Request::nextSequence()
{
    static auto sequenceNumber = QAtomicInteger<Sequence>{};
    return sequenceNumber.fetchAndAddOrdered(1);
}

template<class DataType>
std::optional<DataType> Request::create() const
{
    return DataType{data()};
}

template<class DataType>
std::optional<DataType> Request::get() const
{
    static_assert(DataType::MessageType == Type::Request);

    if (Q_UNLIKELY(type() != Type::Request)) {
        qCWarning(core::logger(this), "Unexpected type (type=%02x)", static_cast<quint8>(type()));
        return {};
    }

    if (Q_UNLIKELY(identifier() != DataType::Identifier)) {
        qCWarning(core::logger(this), "Unexpected identifier (%02x)", static_cast<quint8>(identifier()));
        return {};
    }

    if (Q_UNLIKELY(dataSize() < DataType::MinimumSize)) {
        qCWarning(core::logger(this), "Message too short (%zd byte(s))", dataSize());
        return {};
    }

    if (Q_UNLIKELY(dataSize() > DataType::MaximumSize)) {
        qCWarning(core::logger(this), "Message too long (%zd byte(s))", dataSize());
        return {};
    }

    return create<DataType>();
}

template<>
std::optional<QVariant> Request::get() const
{
    switch (identifier()) {
    case Identifier::Reset:
    case Identifier::GetInterfaceFlags:
        break;

    case Identifier::GetInterfaceInfo:
        if (const auto response = get<GetInterfaceInfoRequest>())
            return QVariant::fromValue(response.value());

        break;

    case Identifier::SetInterfaceInfo:
        if (const auto response = get<SetInterfaceInfoRequest>())
            return QVariant::fromValue(response.value());

        break;

    case Identifier::SetPower:
    case Identifier::SetAcknowledgeMode:
    case Identifier::SetSomeMagic1: // called by DetectESUMode and PowerOn, argument: enum of three values
    case Identifier::Wait:
    case Identifier::SetSomeMagic2: // called by DetectESUMode and PowerOn, argument: some computed value
    case Identifier::GetTrackLoad:
        Q_UNIMPLEMENTED();
        break;

    case Identifier::SendUfm:
        if (const auto response = get<UfmSendRequest>())
            return QVariant::fromValue(response.value());

        break;

    case Identifier::ReceiveUfm:
        if (const auto response = get<UfmReceiveRequest>())
            return QVariant::fromValue(response.value());

        break;

    case Identifier::SendDcc:
        if (const auto response = get<DccRequest>())
            return QVariant::fromValue(response.value());

        break;

    case Identifier::SetLedState:
        if (const auto response = get<SetLedStateRequest>())
            return QVariant::fromValue(response.value());

        break;

    case Identifier::EraseFlash:
    case Identifier::WriteFlash:
    case Identifier::SendMotorola: // ShortPulse, LongPulse, FrontGap, MidGap, RepeatCount, BitCount, Data
        Q_UNIMPLEMENTED();
        break;

    }

    if (dataSize() > 0)
        return data().toByteArray();

    return QVariant{}; // ensure this optional always has a value
}

Request Request::reset(Sequence sequence)
{
    return Request{Identifier::Reset, {}, sequence};
}

Request Request::interfaceFlags(Sequence sequence)
{
    return Request{Identifier::GetInterfaceFlags, {}, sequence};
}

Request Request::interfaceInfo(InterfaceInfo id, Sequence sequence)
{
    return Request{Identifier::GetInterfaceInfo, GetInterfaceInfoRequest{id}.toByteArray(), sequence};
}

Request Request::powerOff(Sequence sequence)
{
    return Request{Identifier::SetPower, PowerSettings::powerOff().toByteArray(), sequence};
}

Request Request::powerOn(PowerSettings settings, Sequence sequence)
{
    if (settings.mode() == PowerSettings::Disabled)
        return powerOff(sequence);

    return Request{Identifier::SetPower, settings.toByteArray(), sequence};
}

Request Request::setAcknowledgeMode(AcknowledgeSettings settings, Sequence sequence)
{
    return Request{Identifier::SetAcknowledgeMode, settings.toByteArray(), sequence};
}

Request Request::sendDcc(DccRequest request, Sequence sequence)
{
    return Request{Identifier::SendDcc, request.toByteArray(), sequence};
}

Request Request::sendUfm(UfmSendRequest request, Sequence sequence)
{
    return Request{Identifier::SendUfm, request.toByteArray(), sequence};
}

Request Request::receiveUfm(UfmReceiveRequest request, Sequence sequence)
{
    return Request{Identifier::ReceiveUfm, request.toByteArray(), sequence};
}

// =====================================================================================================================

template<class DataType>
std::optional<DataType> Response::create() const
{
    return DataType{data().toByteArray()};
}

template<>
std::optional<InterfaceInfoResponse> Response::create() const
{
    if (const auto requestData = request().get<GetInterfaceInfoRequest>())
        return InterfaceInfoResponse{{data().toByteArray()}, requestData->info()};

    return {};
}

template<class DataType>
std::optional<DataType> Response::get() const
{
    static_assert(DataType::MessageType == Type::Response);

    if (Q_UNLIKELY(type() != Type::Response)) {
        qCWarning(core::logger(this), "Unexpected type (type=%02x)", static_cast<quint8>(type()));
        return {};
    }

    if (Q_UNLIKELY(identifier() != DataType::Identifier)) {
        qCWarning(core::logger(this), "Unexpected identifier (%02x)", static_cast<quint8>(identifier()));
        return {};
    }

    if (Q_UNLIKELY(dataSize() < DataType::MinimumSize)) {
        qCWarning(core::logger(this), "Message too short (%d byte(s))", dataSize());
        return {};
    }

    if (Q_UNLIKELY(dataSize() > DataType::MaximumSize)) {
        qCWarning(core::logger(this), "Message too long (%d byte(s))", dataSize());
        return {};
    }

    return create<DataType>();
}

template<>
std::optional<QVariant> Response::get() const
{
    switch (identifier()) {
    case Identifier::DeviceFlagsResponse:
        if (const auto response = get<InterfaceFlagsResponse>())
            return QVariant::fromValue(response.value());

        break;

    case Identifier::ValueResponse:
        if (const auto response = get<InterfaceInfoResponse>())
            return QVariant::fromValue(response.value());
        if (const auto response = get<ValueResponse>())
            return QVariant::fromValue(response.value());

        break;

    case Identifier::DccResponse:
        if (const auto response = get<DccResponse>())
            return QVariant::fromValue(response.value());

        break;
    }

    if (dataSize() > 0)
        return data().toByteArray();

    return QVariant{}; // ensure this optional always has a value
}

// =====================================================================================================================

QDebug operator<<(QDebug debug, Message message)
{
    switch (message.type()) {
    case Message::Type::Request:
        return debug << Request{std::move(message)};
    case Message::Type::Response:
        return debug << Response{Request{}, std::move(message)};
    case Message::Type::Invalid:
        break;
    }

    const auto prettyPrinter = core::PrettyPrinter<decltype(message)>{debug};

    if (message.isValid()) {
        debug.setVerbosity(0);
        debug << "type=" << message.type()
              << ", sequence=" << message.sequence()
              << ", identifier=" << message.identifier();
    } else {
        debug << "Invalid";
    }

    return debug;
}

QDebug operator<<(QDebug debug, Request request)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(request)>{debug};

    debug.setVerbosity(0);
    debug << request.identifier() << ", seq=" << request.sequence();

    if (auto value = request.get<QVariant>().value(); value.typeId() == QMetaType::QByteArray)
        debug << ", " << value.toByteArray().toHex(' ');
    else if (value.isValid())
        debug << ", " << std::move(value);

    return debug;
}

QDebug operator<<(QDebug debug, Response response)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(response)>{debug};
    const auto resposeData = response.get<QVariant>().value();

    debug.setVerbosity(0);
    debug << response.identifier() << ", seq=" << response.sequence();

    if (resposeData.typeId() == QMetaType::QByteArray)
        debug << ", " << resposeData.toByteArray().toHex(' ');
    else if (resposeData.typeId() == qMetaTypeId<DccResponse>())
        debug << ", " << resposeData.value<DccResponse>();
    else if (resposeData.typeId() == qMetaTypeId<InterfaceFlagsResponse>())
        debug << ", " << resposeData.value<InterfaceFlagsResponse>();
    else if (resposeData.typeId() == qMetaTypeId<InterfaceInfo>())
        debug << ", " << resposeData.value<InterfaceInfo>();
    else if (resposeData.typeId() == qMetaTypeId<ValueResponse>())
        debug << ", " << resposeData.value<ValueResponse>();
    else if (resposeData.isValid())
        debug << ", " << std::move(resposeData);

    return debug;
}

// =====================================================================================================================

} // namespace lmrs::esu::lp2
