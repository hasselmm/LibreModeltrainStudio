#ifndef LMRS_ZIMO_MX1_MESSAGE_H
#define LMRS_ZIMO_MX1_MESSAGE_H

#include <lmrs/core/dccconstants.h>
#include <lmrs/core/framestream.h>
#include <lmrs/core/quantities.h>
#include <lmrs/core/typetraits.h>

#include <QDate>
#include <QTypeRevision>
#include <QtEndian>

//FIXME
template<typename T, typename Tag, auto minimum, auto maximum>
struct std::underlying_type<lmrs::core::literal<T, Tag, minimum, maximum>> { using type = T; };

namespace lmrs::zimo::mx1 {

// =====================================================================================================================

//FIXME
template<typename T> struct NativeType;

template<> struct NativeType<quint8>     { using type = quint8; };
template<> struct NativeType<quint16_be> { using type = quint16; };
template<> struct NativeType<quint32_be> { using type = quint32; };

class Message
{
    Q_GADGET

public:
    static constexpr qsizetype ShortHeaderSize = 3;
    static constexpr qsizetype MinimumSize = ShortHeaderSize;

    using Flags = core::literal<quint8, struct FlagsTag>;

    enum class Format : quint8 {
        Short = 0x00,
        Long = 0x80,
    };

    Q_ENUM(Format)

    enum class Type : quint8 {
        Request = 0x00,
        PrimaryResponse = 0x40,
        SecondaryResponse = 0x20,
        SecondaryAcknowledgement = 0x60,
    };

    Q_ENUM(Type)

    enum class Source : quint8 {
        CommandStation = 0x00,
        Host = 0x10,

        MX1 = CommandStation,
        PC = Host,
    };

    Q_ENUM(Source)

    enum class Target : quint8 {
        CommandStation = 0x00,
        AccessoryModule = 0x01,
        TrackSectionModule = 0x02,

        MX1 = CommandStation,
        MX8 = AccessoryModule,
        MX9 = TrackSectionModule,
    };

    Q_ENUM(Target)

    enum class DataFlow : quint8 {
        EndOfData = 0x00,
        MoreData = 0x040,
        MaybeMoreDate = 0x80,
    };

    Q_ENUM(DataFlow)

    using SequenceNumber = core::literal<quint8, struct SequenceNumberTag>;

    Message() noexcept = default;
    Message(Message &&) noexcept = default;
    Message(const Message &) noexcept = default;
    Message &operator=(Message &&) noexcept = default;
    Message &operator=(const Message &) noexcept = default;

    [[nodiscard]] static Message fromData(QByteArray data, SequenceNumber sequence = nextSequence());
    [[nodiscard]] static Message fromFrame(QByteArray frame);

    template<size_t N, typename... Bytes>
    [[nodiscard]] static Message fromData(std::array<quint8, N> bytes, SequenceNumber sequence = nextSequence())
    {
        auto data = QByteArray{reinterpret_cast<const char *>(bytes.data()), static_cast<int>(bytes.size())};
        return fromData(std::move(data), sequence);
    }

    template<typename... Bytes>
    [[nodiscard]] static Message fromData(Bytes... data)
    {
        return fromData(std::array{data...});
    }

    [[nodiscard]] bool isValid() const noexcept;
    [[nodiscard]] bool hasValidChecksum() const noexcept;

    [[nodiscard]] auto sequence() const noexcept { return SequenceNumber{static_cast<quint8>(m_frame[0])}; }
    [[nodiscard]] auto flags() const noexcept { return Flags{static_cast<quint8>(m_frame[1])}; }
    [[nodiscard]] auto format() const noexcept;
    [[nodiscard]] auto type() const noexcept;
    [[nodiscard]] auto source() const noexcept;
    [[nodiscard]] auto target() const noexcept;

    [[nodiscard]] QByteArray toFrame() const noexcept { return m_frame; }
    [[nodiscard]] QByteArray toData() const noexcept;

    [[nodiscard]] quint8 headerSize() const noexcept;
    [[nodiscard]] DataFlow dataFlow() const noexcept;

    [[nodiscard]] quint16 actualChecksum() const noexcept;
    [[nodiscard]] quint16 expectedChecksum() const noexcept;

    template<core::EnumType T>
    [[nodiscard]] static constexpr T masked(Flags flags) noexcept;

protected:
    auto frameSize() const noexcept { return m_frame.size(); }

    template<typename T = quint8>
    typename NativeType<T>::type data(int offset) const noexcept;

    template<typename T = quint8>
    std::optional<typename NativeType<T>::type> optionalData(int offset) const noexcept
    {
        if (frameSize() > qsizetype{offset} + qsizetype{sizeof(T)}) // requested data plus checksum
            return data<T>(offset);

        return {};
    }

private:
    Message(QByteArray frame)
        : m_frame{std::move(frame)}
    {}

    [[nodiscard]] static QByteArray checksum(QByteArrayView data);
    [[nodiscard]] static constexpr quint8 checksum8(QByteArrayView data);
    [[nodiscard]] static constexpr quint16 checksum16(QByteArrayView data);
    [[nodiscard]] static SequenceNumber nextSequence() noexcept;

    QByteArray m_frame;
};

// ---------------------------------------------------------------------------------------------------------------------

template<>
inline quint8 Message::data<quint8>(int offset) const noexcept
{
    return static_cast<quint8>(m_frame[offset]);
}

template<>
inline quint16 Message::data<quint16_be>(int offset) const noexcept
{
    return qFromBigEndian<quint16>(m_frame.constData() + offset);
}

template<>
inline quint32 Message::data<quint32_be>(int offset) const noexcept
{
    return qFromBigEndian<quint32>(m_frame.constData() + offset);
}

// ---------------------------------------------------------------------------------------------------------------------

template<> constexpr Message::Format Message::masked(Flags flags) noexcept { return Format{static_cast<quint8>(flags & 0x80)}; }
template<> constexpr Message::Type   Message::masked(Flags flags) noexcept { return   Type{static_cast<quint8>(flags & 0x60)}; }
template<> constexpr Message::Source Message::masked(Flags flags) noexcept { return Source{static_cast<quint8>(flags & 0x10)}; }
template<> constexpr Message::Target Message::masked(Flags flags) noexcept { return Target{static_cast<quint8>(flags & 0x07)}; }

inline auto Message::format() const noexcept { return masked<Format>(flags()); }
inline auto Message::type() const noexcept { return masked<Type>(flags()); }
inline auto Message::source() const noexcept { return masked<Source>(flags()); }
inline auto Message::target() const noexcept { return masked<Target>(flags()); }

// ---------------------------------------------------------------------------------------------------------------------

template<typename T>
concept MaskedFlag = core::EnumType<T> && requires { Message::masked<T>(Message::Flags{}); };

static_assert(MaskedFlag<Message::Format>);

template<MaskedFlag A, MaskedFlag B>
constexpr auto operator|(A a, B b) noexcept
{
    return Message::Flags{static_cast<quint8>(core::value(a) | core::value(b))};
}

template<MaskedFlag B>
constexpr auto operator|(Message::Flags a, B b) noexcept
{
    return Message::Flags{static_cast<quint8>(core::value(a) | core::value(b))};
}

// =====================================================================================================================

class Request : public Message
{
    Q_GADGET

public:
    enum class Code : quint8 {
        Reset = 0x00,
        PowerControl = 2,
        VehicleControl = 3,
        InvertFunctions = 4,
        Accelerate = 5,
        Deceleration = Accelerate,
        ShuttleControl = 6,
        AccessoryControl = 7,
        QueryVehicle = 8,
        QueryAccessory = 9,
        AddressControl = 10,
        QueryStationStatus = 11,
        ReadStationVariable = 12,
        WriteStationVariable = ReadStationVariable,
        QueryStationEquipment = 13,
        SerialToolInfo = 17,
        VariableControl = 19,
        AccessoryChanged = 254,
        VehicleChanged = 255,
    };

    Q_ENUM(Code)

    enum class PowerControl : quint8 {
        EmergencyStop = 0,
        PowerOff = 1,
        PowerOn = 2,
        Query = 3,
    };

    Q_ENUM(PowerControl)

    enum class SerialTool : quint8 {
        Default = 0,
        ZimoServiceTool,
        FreiwaldTrainController,
        KamTrainServer,
        SperrerAdapt,
        SperrerSDTP,
        Zirc,
    };

    Q_ENUM(SerialTool)

    enum class SerialToolAction : quint8 {
        RefreshCommunication = 0,
        StartCommunication = 1,
        RefreshStopCommunication = 2,
    };

    Q_ENUM(SerialToolAction)

    enum class AddressType : quint8 {
        DCC = 0x80,
        Motorola = 0x40,
    };

    Q_ENUM(AddressType)

    Request() = default;
    Request(Request &&request) noexcept = default;
    Request(const Request &request) noexcept = default;
    Request(Message &&message) noexcept : Message{message} { Q_ASSERT(type() == Type::Request); }
    Request(const Message &message) noexcept : Message{message} { Q_ASSERT(type() == Type::Request); }

    Request &operator=(Request &&request) noexcept = default;
    Request &operator=(const Request &request) noexcept = default;

    [[nodiscard]] auto code() const noexcept { return Code{data(2)}; }

    [[nodiscard]] static Request reset() noexcept;
    [[nodiscard]] static Request powerControl(PowerControl control);
    [[nodiscard]] static Request requestEmergencyStop() noexcept { return powerControl(PowerControl::EmergencyStop); }
    [[nodiscard]] static Request powerOff() noexcept { return powerControl(PowerControl::PowerOff); }
    [[nodiscard]] static Request powerOn() noexcept { return powerControl(PowerControl::PowerOn); }
    [[nodiscard]] static Request queryPowerState() noexcept { return powerControl(PowerControl::Query); }
    [[nodiscard]] static Request vehicleControl(core::dcc::VehicleAddress address, core::dcc::Speed speed,
                                                core::dcc::Direction direction = core::dcc::Direction::Forward,
                                                lmrs::core::dcc::FunctionState functions = {}) noexcept;
    [[nodiscard]] static Request queryVehicle(core::dcc::VehicleAddress address) noexcept;
    [[nodiscard]] static Request readVariable(core::dcc::VehicleAddress address,
                                              core::dcc::VariableIndex variable) noexcept;
    [[nodiscard]] static Request writeVariable(core::dcc::VehicleAddress address,
                                               core::dcc::VariableIndex variable,
                                               core::dcc::VariableValue value) noexcept;
    [[nodiscard]] static Request serialToolInfo(SerialToolAction action, SerialTool tool = SerialTool::Default);
    [[nodiscard]] static Request startCommunication(SerialTool tool = SerialTool::Default);
    [[nodiscard]] static Request refreshCommunication(SerialTool tool = SerialTool::Default);
    [[nodiscard]] static Request stopCommunication(SerialTool tool = SerialTool::Default);
    [[nodiscard]] static Request queryStationStatus(Target target = Target::CommandStation);
    [[nodiscard]] static Request queryStationEquipment(Target target = Target::CommandStation);
};

// =====================================================================================================================

class Response : public Message
{
    Q_GADGET

public:
    enum class Status {
        Unknown = -1,
        Succeeded,
        InvalidAddress,
        InvalidAddressIndex,
        ForwardingFailed,
        Busy,
        MotorolaDisabled,
        DccDisabled,
        InvalidVariable,
        InvalidSection,
        ModuleNotFound,
        InvalidMessage,
        InvalidSpeed,
    };

    Q_ENUM(Status)

    using Message::Message;

    Response(Message &&message) noexcept
        : Message{message}
    {
        Q_ASSERT(type() == Type::PrimaryResponse
                 || type() == Type::SecondaryResponse
                 || type() == Type::SecondaryAcknowledgement);
    }

    Response(const Message &message) noexcept
        : Message{message}
    {
        Q_ASSERT(type() == Type::PrimaryResponse
                 || type() == Type::SecondaryResponse
                 || type() == Type::SecondaryAcknowledgement);
    }

    [[nodiscard]] Request::Code requestCode() const noexcept;
    [[nodiscard]] SequenceNumber requestSequence() const noexcept;
    [[nodiscard]] Status status() const noexcept;
    [[nodiscard]] qsizetype dataSize() const noexcept;

    [[nodiscard]] static Response secondaryAcknowledgement(SequenceNumber requestSequence, Request::Code requestCode);
};

// ---------------------------------------------------------------------------------------------------------------------

template<Request::Code expectedRequestCode>
class SpecificReponse : public Response
{
public:
    SpecificReponse(Response &&response) noexcept : Response{response} { Q_ASSERT(requestCode() == expectedRequestCode); }
    SpecificReponse(const Response &response) noexcept : Response{response} { Q_ASSERT(requestCode() == expectedRequestCode); }
};

// ---------------------------------------------------------------------------------------------------------------------

class PowerControlResponse : public SpecificReponse<Request::Code::PowerControl>
{
    Q_GADGET

public:
    enum class Status : quint8 {
        DCC = 0x80,
        Motorola = 0x40,
        UES = 0x04,
        TrackVoltage = 0x02,
        BroadcastStopped = 0x01,
    };

    Q_FLAG(Status)
    Q_DECLARE_FLAGS(StatusFlags, Status);

    using SpecificReponse::SpecificReponse;

    StatusFlags status() const noexcept { return StatusFlags{data(4)}; }
};

// ---------------------------------------------------------------------------------------------------------------------

class SerialToolInfoReponse : public SpecificReponse<Request::Code::SerialToolInfo>
{
    Q_GADGET

public:
    using SpecificReponse::SpecificReponse;
};

// ---------------------------------------------------------------------------------------------------------------------

class StationStatusReponse : public SpecificReponse<Request::Code::QueryStationStatus>
{
    Q_GADGET

public:
    using SpecificReponse::SpecificReponse;

    auto target() const noexcept { return Request::Target{data(4)}; }
    auto current1() const noexcept { return core::milliamperes{data<quint16_be>(5) * 10}; };
    auto voltage1() const noexcept { return core::millivolts{data<quint8>(7) * 100}; };
    auto current2() const noexcept { return core::milliamperes{data<quint16_be>(8) * 10}; };
    auto voltage2() const noexcept { return core::millivolts{data<quint8>(10) * 100}; };
//    12 cAux auxiliary inputs
//
//    Current values with bit 15 set are special values:
//    special value decription
//    0x8000 no 2nd value (MX1EC)
//    0x8001 “OFF”
//    0x8002 “UEP”
//    0x8003 “UES”
//    0x8004 “AUS”
//    0x8005 “SSP”
//    0x8006 “No Si”
//    0x8007 “SL UES”
};

// ---------------------------------------------------------------------------------------------------------------------

class StationEquipmentReponse : public SpecificReponse<Request::Code::QueryStationEquipment>
{
    Q_GADGET

public:
    enum class DeviceId : quint8 {
        MX1_2000HS = 1,
        MX1_2000EC = 2,
        MX31ZL = 3,
        MXULF = 4,
    };

    Q_ENUM(DeviceId);

    using SpecificReponse::SpecificReponse;

    auto canAddress() const noexcept { return data<quint16_be>(5); }
    auto deviceId() const noexcept { return DeviceId{data(7)}; }
    auto romSize() const noexcept { return data<quint16_be>(8); } // FIXME: bytes unit, also for download progress
    auto switches() const noexcept { return data(18); }
    auto target() const noexcept { return Request::Target{data(23)}; }

    auto hardwareVersion() const noexcept { return makeVersion(data(10), data(11)); }
    auto firmwareVersion() const noexcept { return makeVersion(data(12), data(13), data(19)); }
    auto bootloaderVersion() const noexcept { return makeVersion(data(20), data(21), data(22)); }

    auto firmwareDate() const noexcept { return QDate{data(16) * 100 + data(17), data(15), data(14)}; }
    auto serialNumber() const noexcept { return optionalData<quint32_be>(24); }

private:
    QVersionNumber makeVersion(int major, int minor, int micro = 0) const noexcept;
};

// ---------------------------------------------------------------------------------------------------------------------

class VariableControlResponse : public SpecificReponse<Request::Code::VariableControl>
{
    Q_GADGET

public:
    using SpecificReponse::SpecificReponse;

    auto vehicle() const noexcept { return core::dcc::VehicleAddress{static_cast<quint16>(data<quint16_be>(4) & 0x3fff)}; }
    auto variable() const noexcept { return core::dcc::VariableIndex{data<quint16_be>(6)}; }
    auto value() const noexcept { return data(8); }
};

// =====================================================================================================================

using FrameFormat = core::StaticFrameFormat<0x01, 0x17, 0x10, 0x20>;

static_assert(FrameFormat::start()       == 0x01);
static_assert(FrameFormat::startLength() ==    2);
static_assert(FrameFormat::stop()        == 0x17);
static_assert(FrameFormat::stopLength()  ==    1);
static_assert(FrameFormat::escape()      == 0x10);
static_assert(FrameFormat::mask()        == 0x20);

using StreamReader = core::GenericFrameStreamReader<FrameFormat, Message::ShortHeaderSize>;
using StreamWriter = core::GenericFrameStreamWriter<FrameFormat>;

// =====================================================================================================================

QDebug operator<<(QDebug debug, const Message &message);
QDebug operator<<(QDebug debug, const Request &request);
QDebug operator<<(QDebug debug, const Response &response);

} // namespace lmrs::zimo::mx1

#endif // LMRS_ZIMO_MX1_MESSAGE_H
