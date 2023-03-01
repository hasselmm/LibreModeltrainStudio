#ifndef LMRS_ESU_LP2MESSAGE_H
#define LMRS_ESU_LP2MESSAGE_H

#include <QMetaType>

class QDebug;

namespace lmrs::esu::lp2 {

struct AcknowledgeSettings;
struct DccRequest;
struct DccSettings;
struct PowerSettings;
struct UfmReceiveRequest;
struct UfmSendRequest;
struct UfmSettings;

enum class InterfaceInfo : quint8;

class Message
{
    Q_GADGET

public:
    static constexpr auto HeaderSize = qsizetype{3}; // type, sequence, message-id

    enum class Type : quint8 {
        Invalid     = 0x00,
        Request     = 0x01,
        Response    = 0x02,
    };

    Q_ENUM(Type)

    using Identifier = quint8;
    using Sequence = quint8;

    Message() = default;

    explicit Message(QByteArray data);
    explicit Message(Type type, Sequence sequence, Identifier identifier, QByteArray data);

    bool isValid() const;

    Type type() const;
    Sequence sequence() const;
    Identifier identifier() const;
    QByteArrayView data() const;
    int dataSize() const;

    QByteArray toByteArray() const;

private:
    QByteArray m_data;
};

class Request : public Message
{
    Q_GADGET

public:
    enum class Identifier : quint8 {
        Reset               = 0x00,
        GetInterfaceFlags   = 0x01,
        GetInterfaceInfo    = 0x02,
        SetInterfaceInfo    = 0x03,

        EraseFlash          = 0x06,
        WriteFlash          = 0x07,

        SetPower            = 0x10,
        GetTrackLoad        = 0x12,
        SetAcknowledgeMode  = 0x14,
        SetSomeMagic1       = 0x16, // called by DetectESUMode and PowerOn, argument: enum of three values
        Wait                = 0x18,
        SetSomeMagic2       = 0x19, // called by DetectESUMode and PowerOn, argument: some computed value

        SendUfm             = 0x2a,
        ReceiveUfm          = 0x2b,

        SendMotorola        = 0x30, // ShortPulse, LongPulse, FrontGap, MidGap, RepeatCount, BitCount, Data
        SendDcc             = 0x34, // ShortPulse, LongPulse, PreambleBits, StopBits, RepeatCount, ExtCount, Data

        SetLedState         = 0x4c,
    };

    Q_ENUM(Identifier)

    Request() = default;

    explicit Request(QByteArray data) : Message{std::move(data)} {}
    explicit Request(Message message) : Message{std::move(message)} {}
    explicit Request(Identifier identifier, QByteArray data, Sequence sequence = nextSequence());

    auto identifier() const { return static_cast<Identifier>(Message::identifier()); }
    bool isValid() const { return Message::isValid() && type() == Type::Request; }

    static Sequence nextSequence();

    static Request reset(Sequence sequence = nextSequence());
    static Request interfaceFlags(Sequence sequence = nextSequence());
    static Request interfaceInfo(InterfaceInfo id, Sequence sequence = nextSequence());
    static Request powerOff(Sequence sequence = nextSequence());
    static Request powerOn(PowerSettings settings, Sequence sequence = nextSequence());
    static Request setAcknowledgeMode(AcknowledgeSettings settings, Sequence sequence = nextSequence());
    static Request sendDcc(DccRequest request, Sequence sequence = nextSequence());
    static Request sendUfm(UfmSendRequest request, Sequence sequence = nextSequence());
    static Request receiveUfm(UfmReceiveRequest request, Sequence sequence = nextSequence());

    template<class DataType> std::optional<DataType> get() const;

    template<Identifier identifier, int minimumSize, int maximumSize = minimumSize>
    class Data
    {
    public:
        static constexpr auto MessageType = Type::Request;
        static constexpr auto Identifier  = identifier;
        static constexpr auto MinimumSize = minimumSize;
        static constexpr auto MaximumSize = maximumSize;

        static constexpr auto Size = (std::conditional<MinimumSize == MaximumSize,
                                      std::integral_constant<int, MinimumSize>,
                                      std::integral_constant<std::nullptr_t, nullptr>>::type::value);

        Data() = default;

        explicit Data(QByteArrayView data) : m_data{data.toByteArray()} {}

        bool isValid() const { return m_data.size() >= MinimumSize && m_data.size() <= MaximumSize; }
        auto toByteArray() const { return m_data; }

    protected:
        QByteArray m_data;
    };

private:
    template<class DataType> std::optional<DataType> create() const;
};

class Response : public Message
{
    Q_GADGET

public:
    enum class Identifier : quint8 {
        DeviceFlagsResponse = 0x01,
        ValueResponse       = 0x05,
        DccResponse         = 0x07,
    };

    Q_ENUM(Identifier)

    enum class Status : quint8 {
        Success             = 0x00,
        UnknownError        = 0x01,
        InvalidRequest      = 0x02,
        InvalidArgument     = 0x03,
        OperationFailed     = 0x04,
        InvalidSequence     = 0x05,
        IncompletePacket    = 0x06,
        InvalidChecksum     = 0x07,
        Overcurrent         = 0x08,
    };

    Q_ENUM(Status)

    Response() = default;

    explicit Response(Request request, Message response)
        : Message{std::move(response)}
        , m_request{std::move(request)}
    {}

    auto identifier() const { return static_cast<Identifier>(Message::identifier()); }
    bool isValid() const { return Message::isValid() && type() == Type::Response; }
    auto request() const { return m_request; }

    template<class DataType> std::optional<DataType> get() const;

    template<Identifier identifier, int minimumSize, int maximumSize = minimumSize>
    struct Data
    {
        static constexpr auto MessageType = Type::Response;
        static constexpr auto Identifier = identifier;
        static constexpr auto MinimumSize = minimumSize;
        static constexpr auto MaximumSize = maximumSize;

        static constexpr auto Size = (std::conditional<MinimumSize == MaximumSize,
                                      std::integral_constant<int, MinimumSize>,
                                      std::integral_constant<std::nullptr_t, nullptr>>::type::value);

        QByteArray data; // FIXME: make private
    };

private:
    template<class DataType> std::optional<DataType> create() const;

    Request m_request;
};

QDebug operator<<(QDebug, Message);
QDebug operator<<(QDebug, Request);
QDebug operator<<(QDebug, Response);

} // namespace lmrs::esu::lp2

#endif // LMRS_ESU_LP2MESSAGE_H
