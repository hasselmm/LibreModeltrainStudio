#include "lp2request.h"

#include <lmrs/core/dccrequest.h>
#include <lmrs/core/logging.h>
#include <lmrs/core/userliterals.h>

#include <QtEndian>

namespace lmrs::esu::lp2 {

namespace {

template<typename... Values>
QByteArray makeData(Values... values)
{
    const char data[] = {static_cast<char>(values)...};
    return QByteArray{data, sizeof data};
}

constexpr quint8 ufmMode(bool precisionMode, UfmSettings::DataBits dataBits,
                         UfmSettings::Parity parity, UfmSettings::StopBits stopBits)
{
    return (precisionMode ? 0x80_u8 : 0x00_u8)
            | static_cast<quint8>(core::value(dataBits) << 4)
            | static_cast<quint8>(core::value(parity) << 2)
            | static_cast<quint8>(stopBits);
}

static_assert(ufmMode(false, UfmSettings::DataBits::Data8, UfmSettings::Parity::Even, UfmSettings::StopBits::Stop1) == 0x79);

} // namespace

GetInterfaceInfoRequest::GetInterfaceInfoRequest(InterfaceInfo info)
    : Data{makeData(info)}
{}

SetLedStateRequest::SetLedStateRequest(Light light, Duration active, Duration inactive)
    : Data{makeData(light, active.count(), inactive.count())}
{}

SetInterfaceInfoRequest::SetInterfaceInfoRequest(InterfaceInfo info, quint32 value)
    : Data{QByteArray{5, Qt::Uninitialized}}
{
    auto data = m_data.data();
    qToLittleEndian(info, &data[0]);
    qToLittleEndian(value, &data[1]);
}

quint32 SetInterfaceInfoRequest::value() const
{
    return qFromLittleEndian<quint32>(m_data.constData() + 1);
}

PowerSettings::PowerSettings(Mode mode, Current currentLimit, Voltage voltage, bool reserved)
    : Data{makeData(mode, reserved ? 1 : 0, currentLimit, voltage)}
{}

PowerSettings PowerSettings::driving(Current currentLimit, Voltage voltage)
{
    return PowerSettings{Enabled, currentLimit, voltage, false};
}

PowerSettings PowerSettings::service(Current currentLimit, Voltage voltage)
{
    return PowerSettings{Service, currentLimit, voltage, false};
}

PowerSettings PowerSettings::powerOff()
{
    static auto settings = PowerSettings{Disabled, 0, 0, false};
    return settings;
}

AcknowledgeSettings::AcknowledgeSettings(Duration minimumPeriod, Duration maximumPeriod,
                                         AcknowledgeLevel acknowledgeLevel)
    : Data{makeData(minimumPeriod.count(), maximumPeriod.count(), acknowledgeLevel)}
{}

DccSettings::DccSettings(Duration shortPulse, Duration longPulse, quint8 preambleBits,
                         quint8 stopBits, quint8 repeatCount, quint8 extCount)
    : Data{makeData(shortPulse.count(), longPulse.count(), preambleBits, stopBits, repeatCount, extCount)}
{}

DccRequest::DccRequest(DccSettings settings, QByteArray request)
    : Data{settings.toByteArray() + request}
{}

DccRequest::DccRequest(DccSettings settings, dcc::Request request)
    : Data{settings.toByteArray() + request.toByteArray()}
{}

dcc::Request DccRequest::dcc() const
{
    return dcc::Request{data()};
}

DccRequest DccRequest::reset(quint8 repeatCount)
{
    auto settings = DccSettings::nack();
    settings.setRepeatCount(repeatCount);
    return DccRequest{std::move(settings), dcc::Request::reset()};
}

DccRequest DccRequest::setSpeed14(quint16 address, quint8 speed, dcc::Direction direction, bool light)
{
    return DccRequest{DccSettings::nack(), dcc::Request::setSpeed14(address, speed, direction, light)};
}

DccRequest DccRequest::setSpeed28(quint16 address, quint8 speed, dcc::Direction direction)
{
    return DccRequest{DccSettings::nack(), dcc::Request::setSpeed28(address, speed, direction)};
}

DccRequest DccRequest::setSpeed126(quint16 address, quint8 speed, dcc::Direction direction)
{
    return DccRequest{DccSettings::nack(), dcc::Request::setSpeed126(address, speed, direction)};
}

DccRequest DccRequest::setFunctions(quint16 address, dcc::FunctionGroup group, quint8 functions)
{
    return DccRequest{DccSettings::nack(), dcc::Request::setFunctions(address, group, functions)};
}

DccRequest DccRequest::verifyBit(quint16 variable, bool value, quint8 position)
{
    return DccRequest{DccSettings::ack(), dcc::Request::verifyBit(variable, value, position)};
}

DccRequest DccRequest::verifyByte(quint16 variable, quint8 value)
{
    return DccRequest{DccSettings::ack(), dcc::Request::verifyByte(variable, value)};
}

DccRequest DccRequest::writeByte(quint16 variable, quint8 value)
{
    return DccRequest{DccSettings::ack(), dcc::Request::writeByte(variable, value)};
}

UfmSettings::UfmSettings(Duration bitTime, bool precisionMode, DataBits dataBits, Parity parity, StopBits stopBits)
    : Data{makeData(bitTime.count(), ufmMode(precisionMode, dataBits, parity, stopBits))}
{}

UfmSendRequest::UfmSendRequest(UfmSettings settings, QByteArray ufmData)
    : Data{settings.toByteArray() + ufmData}
{}

UfmReceiveRequest::UfmReceiveRequest(UfmSettings settings, Duration timeout)
    : Data{settings.toByteArray() + makeData(timeout.count())}
{}

QDebug operator<<(QDebug debug, DccRequest request)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(request)>{debug};
    const auto dcc = request.dcc();

    debug.setVerbosity(0);
    debug << request.settings() << ", " << dcc.toByteArray().toHex(' ') << ' ' << dcc;

    return debug;
}

QDebug operator<<(QDebug debug, DccSettings settings)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(settings)>{debug};
    static const auto defaultSettings = DccSettings::drive();

    auto separator = core::SeparatorState{};

    if (settings.shortPulse() != defaultSettings.shortPulse())
        debug << separator << "short-pulse=" << settings.shortPulse();
    if (settings.longPulse() != defaultSettings.longPulse())
        debug << separator << "long-pulse=" << settings.longPulse();
    if (settings.preambleBits() != defaultSettings.preambleBits())
        debug << separator << "preamble=" << settings.preambleBits();
    if (settings.stopBits() != defaultSettings.stopBits())
        debug << separator << "stop-bits=" << settings.stopBits();
    if (settings.repeatCount() != defaultSettings.repeatCount())
        debug << separator << "repeat=" << settings.repeatCount();
    if (settings.extCount() != defaultSettings.extCount())
        debug << separator << "ext=" << settings.extCount();

    return debug;
}

QDebug operator<<(QDebug debug, GetInterfaceInfoRequest request)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(request)>{debug};
    return debug << request.info();
}

QDebug operator<<(QDebug debug, PowerSettings request)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(request)>{debug};

    debug << request.mode()
          << ", current limit=" << request.currentLimit()
          << ", voltage=" << request.voltage();

    if (request.reserved())
        debug << ", reserved=" << request.reserved();

    return debug;
}

QDebug operator<<(QDebug debug, SetInterfaceInfoRequest request)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(request)>{debug};
    return debug << request.info() << ", value=" << request.value();
}

QDebug operator<<(QDebug debug, SetLedStateRequest request)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(request)>{debug};

    return debug << request.light()
                 << ", on=" << request.activePeriod()
                 << ", off=" << request.inactivePeriod();
}

QDebug operator<<(QDebug debug, UfmReceiveRequest request)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(request)>{debug};

    debug.setVerbosity(0);
    debug << request.settings() << ", timeout" << request.timeout();

    return debug;
}

QDebug operator<<(QDebug debug, UfmSendRequest request)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(request)>{debug};
    const auto ufm = request.data();

    debug.setVerbosity(0);
    debug << request.settings() << ", " << ufm.toHex(' ');

    return debug;
}

QDebug operator<<(QDebug debug, UfmSettings settings)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(settings)>{debug};

    debug.setVerbosity(0);
    debug << settings.bitTime() << ", "
          << settings.dataBits() << ", "
          << settings.parity() << ", "
          << settings.stopBits();

    if (settings.precisionMode())
        debug << ", precision-mode";

    return debug;
}

} // namespace lmrs::esu::lp2
