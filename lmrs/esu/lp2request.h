#ifndef LMRS_ESU_LP2REQUEST_H
#define LMRS_ESU_LP2REQUEST_H

#include "lp2constants.h"
#include "lp2message.h"

#include <chrono>

class QDebug;

namespace lmrs::core::dcc {
class Request;
}

namespace lmrs::esu::lp2 {

using namespace std::chrono_literals;

struct GetInterfaceInfoRequest : public Request::Data<Request::Identifier::GetInterfaceInfo, 1>
{
public:
    using Data::Data;
    explicit GetInterfaceInfoRequest(InterfaceInfo info);

    auto info() const { return static_cast<InterfaceInfo>(m_data[0]); }
};

struct SetInterfaceInfoRequest : public Request::Data<Request::Identifier::SetInterfaceInfo, 5>
{
    using Data::Data;
    explicit SetInterfaceInfoRequest(InterfaceInfo info, quint32 value);

    auto info() const { return static_cast<InterfaceInfo>(m_data[0]); }
    quint32 value() const;
};

struct PowerSettings : public Request::Data<Request::Identifier::SetPower, 4>
{
    enum Mode : quint8
    {
        Disabled    = 0x00,
        Enabled     = 0x01,
        Service     = 0x02,
    };

    using Current = quint8; // FIXME: maybe define proper Ampere type with accurate scaling, just like std::chrono::duration
    using Voltage = quint8; // FIXME: maybe define proper Volts type with accurate scaling, just like std::chrono::duration

    using Data::Data;
    PowerSettings() : PowerSettings{Disabled, 0, 0} {}
    explicit PowerSettings(Mode mode, Current currentLimit = 40, Voltage voltage = 25, bool reserved = false);

    auto mode() const { return static_cast<Mode>(m_data[0]); }
    auto reserved() const { return m_data[1] ? true : false; }
    auto currentLimit() const { return static_cast<Current>(m_data[2]); }
    auto voltage() const { return static_cast<Voltage>(m_data[3]); }

    static PowerSettings driving(Current currentLimit = 45, Voltage voltage = 25);
    static PowerSettings service(Current currentLimit = 40, Voltage voltage = 25);
    static PowerSettings powerOff();

    Q_GADGET
    Q_ENUM(Mode)
};

struct AcknowledgeSettings : public Request::Data<Request::Identifier::SetAcknowledgeMode, 3>
{
    using Duration = std::chrono::duration<quint8, std::milli>;
    using AcknowledgeLevel = quint8; // FIXME: figure out what this is: a current, a voltage, what?

    using Data::Data;
    AcknowledgeSettings() : AcknowledgeSettings{1ms, 12ms, 5} {}
    explicit AcknowledgeSettings(Duration minimumPeriod, Duration maximumPeriod, AcknowledgeLevel acknowledgeLevel);

    auto minimumPeriod() const { return Duration{static_cast<quint8>(m_data[0])}; }
    auto maximumPeriod() const { return Duration{static_cast<quint8>(m_data[1])}; }
    auto acknowledgeLevel() const { return static_cast<AcknowledgeLevel>(m_data[2]); }
};

struct DccSettings : public Request::Data<Request::Identifier::SendDcc, 6>
{
    using Duration = std::chrono::duration<quint8, std::micro>;

    using Data::Data;
    explicit DccSettings(Duration shortPulse, Duration longPulse, quint8 preambleBits,
                         quint8 stopBits, quint8 repeatCount = 1, quint8 extCount = 0);

    void setRepeatCount(quint8 repeatCount) { m_data[4] = static_cast<char>(repeatCount); }

    auto shortPulse() const { return Duration{static_cast<quint8>(m_data[0])}; }
    auto longPulse() const { return Duration{static_cast<quint8>(m_data[1])}; }
    auto preambleBits() const { return static_cast<quint8>(m_data[2]); }
    auto stopBits() const { return static_cast<quint8>(m_data[3]); }
    auto repeatCount() const { return static_cast<quint8>(m_data[4]); }
    auto extCount() const { return static_cast<quint8>(m_data[5]); }

    static auto afb() { static const auto settings = DccSettings{58us, 116us, 18, 1, 3, 255}; return settings; }
    static auto helper() { static const auto settings = DccSettings{58us, 116us, 20, 1}; return settings; }
    static auto nack() { static const auto settings = DccSettings{58us, 100us, 18, 2}; return settings; }
    static auto ack() { static const auto settings = DccSettings{58us, 100us, 18, 2, 5, 5}; return settings; }
    static auto drive() { static const auto settings = DccSettings{58us, 116us, 20, 1}; return settings; }
};

struct DccRequest : public Request::Data<Request::Identifier::SendDcc, DccSettings::Size + 3, 256>
{
    using Data::Data;
    explicit DccRequest(DccSettings settings, QByteArray request);
    explicit DccRequest(DccSettings settings, dcc::Request request);

    auto settings() const { return DccSettings{m_data}; }
    auto data() const { return m_data.mid(DccSettings::Size); }
    dcc::Request dcc() const;

    static DccRequest reset(quint8 repeatCount = 1);
    static DccRequest setSpeed14(quint16 address, quint8 speed, dcc::Direction direction, bool light);
    static DccRequest setSpeed28(quint16 address, quint8 speed, dcc::Direction direction);
    static DccRequest setSpeed126(quint16 address, quint8 speed, dcc::Direction direction);
    static DccRequest setFunctions(quint16 address, dcc::FunctionGroup group, quint8 functions);
    static DccRequest verifyBit(quint16 variable, bool value, quint8 position);
    static DccRequest verifyByte(quint16 variable, quint8 value);
    static DccRequest writeByte(quint16 variable, quint8 value);
};

struct UfmSettings : public Request::Data<Request::Identifier::SendUfm, 2>
{
    using Duration = std::chrono::duration<quint8, std::micro>;

    enum class DataBits { Data8 = 7 };
    Q_ENUM(DataBits)

    enum class Parity { None = 0, Even = 2, Odd = 3 };
    Q_ENUM(Parity)

    enum class StopBits { Stop0 = 0, Stop1 = 1, Stop2 = 2 };
    Q_ENUM(StopBits)

    using Data::Data;
    UfmSettings() : UfmSettings{20us, false, DataBits::Data8, Parity::Even, StopBits::Stop2} {}
    explicit UfmSettings(Duration bitTime, bool precisionMode, DataBits dataBits, Parity parity, StopBits stopBits);

    auto bitTime() const { return Duration{static_cast<quint8>(m_data[0])}; }
    auto precisionMode() const { return static_cast<quint8>(m_data[1]) & 0x80 ? true : false; }
    auto dataBits() const { return static_cast<DataBits>((static_cast<quint8>(m_data[1]) >> 4) & 7); }
    auto parity() const { return static_cast<Parity>((static_cast<quint8>(m_data[1]) >> 2) & 3); }
    auto stopBits() const { return static_cast<StopBits>(static_cast<quint8>(m_data[1]) & 3); }

    Q_GADGET
};

struct UfmSendRequest : public Request::Data<Request::Identifier::SendUfm, UfmSettings::Size, 256>
{
    using Data::Data;
    explicit UfmSendRequest(QByteArray ufmData) : UfmSendRequest{{}, std::move(ufmData)} {}
    explicit UfmSendRequest(UfmSettings settings, QByteArray ufmData);

    auto settings() const { return UfmSettings{m_data}; }
    auto data() const { return m_data.mid(UfmSettings::Size); }
    // ufm::Request ufm() const;
};

struct UfmReceiveRequest : public Request::Data<Request::Identifier::SendUfm, UfmSettings::Size, 256>
{
    using Duration = std::chrono::duration<quint8, std::micro>;

    using Data::Data;
    explicit UfmReceiveRequest(Duration timeout) : UfmReceiveRequest{{}, timeout} {}
    explicit UfmReceiveRequest(UfmSettings settings, Duration timeout);

    auto settings() const { return UfmSettings{m_data}; }
    auto timeout() const { return Duration{static_cast<quint8>(m_data[UfmSettings::Size])}; }
};

struct SetLedStateRequest : public Request::Data<Request::Identifier::SetLedState, 3>
{
    enum class Light : quint8 {
        Yellow = 0x00,
        Green = 0x01,
    };

    Q_ENUM(Light)

    using Duration = std::chrono::duration<quint8, std::milli>;

    using Data::Data;
    explicit SetLedStateRequest(Light light, Duration active, Duration inactive);

    auto light() const { return static_cast<Light>(m_data[0]); }
    auto activePeriod() const { return Duration{static_cast<quint8>(m_data[1])}; }
    auto inactivePeriod() const { return Duration{static_cast<quint8>(m_data[2])}; }

    Q_GADGET
};

QDebug operator<<(QDebug, DccRequest);
QDebug operator<<(QDebug, DccSettings);
QDebug operator<<(QDebug, GetInterfaceInfoRequest);
QDebug operator<<(QDebug, PowerSettings);
QDebug operator<<(QDebug, SetInterfaceInfoRequest);
QDebug operator<<(QDebug, SetLedStateRequest);
QDebug operator<<(QDebug, UfmReceiveRequest);
QDebug operator<<(QDebug, UfmSendRequest);
QDebug operator<<(QDebug, UfmSettings);

} // namespace lmrs::esu::lp2

#endif // LMRS_ESU_LP2REQUEST_H
