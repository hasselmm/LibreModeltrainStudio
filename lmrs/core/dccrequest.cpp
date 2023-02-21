#include "dccrequest.h"

#include "logging.h"
#include "userliterals.h"

#include <QLoggingCategory>

#include <QtEndian>

namespace lmrs::core::dcc {

namespace {

template <size_t N>
void updateChecksum(std::array<quint8, N> &packet)
{
    quint8 checksum = 0;

    for (auto it = packet.begin(); it != packet.end() - 1; ++it)
        checksum ^= *it;

    packet.back() = checksum;
}

QByteArray multiFunctionPacket(quint16 address, quint8 command)
{
    if (address < 128) {
        std::array<quint8, 3> packet = {static_cast<quint8>(address), command, 0};
        updateChecksum(packet);

        return {reinterpret_cast<const char *>(packet.data()), packet.size()};
    } else {
        const auto addrHigh = static_cast<quint8>((address >> 8) | 0xc0);
        const auto addrLow = static_cast<quint8>(address & 255);

        std::array<quint8, 4> packet = {addrHigh, addrLow, command, 0};
        updateChecksum(packet);

        return {reinterpret_cast<const char *>(packet.data()), packet.size()};
    }
}

QByteArray multiFunctionPacket(quint16 address, quint8 command, quint8 data)
{
    if (address < 128) {
        std::array<quint8, 4> packet = {static_cast<quint8>(address), command, data, 0};
        updateChecksum(packet);

        return {reinterpret_cast<const char *>(packet.data()), packet.size()};
    } else {
        const auto addrHigh = static_cast<quint8>((address >> 8) | 0xc0);
        const auto addrLow = static_cast<quint8>(address & 255);

        std::array<quint8, 5> packet = {addrHigh, addrLow, command, data, 0};
        updateChecksum(packet);

        return {reinterpret_cast<const char *>(packet.data()), packet.size()};
    }
}

} // namespace

bool Request::hasExtendedAddress() const
{
    return !!(static_cast<quint8>(m_data[0]) & 0xc0);
}

quint16 Request::address() const
{
    if (hasExtendedAddress())
        return qFromBigEndian<quint16>(m_data.constData()) & 0x3fff;

    return m_data[0] & 0x7f;
}

Request Request::reset()
{
    return Request{"00 00 00"_hex};
}

Request Request::setSpeed14(quint16 address, quint8 speed, dcc::Direction direction, bool light)
{
    // FIXME: find better handling for Stop = 0b0000, and E-Stop = 0b0001
    if (speed > 15) {
        qCWarning(logger<Request>(), "Speed value out of range [0..15]");
        return {};
    }

    switch (direction) {
    case dcc::Direction::Forward:
        return Request{multiFunctionPacket(address, (light ? 0b011'1'0000_u8 : 0b011'0'0000_u8) | speed)};
    case dcc::Direction::Reverse:
        return Request{multiFunctionPacket(address, (light ? 0b010'1'0000_u8 : 0b010'0'0000_u8) | speed)};
    case dcc::Direction::Unknown:
        break;
    }

    qCWarning(logger<Request>(), "Invalid direction");
    return {};
}

Request Request::setSpeed28(quint16 address, quint8 speed, dcc::Direction direction)
{
    // FIXME: Deal with this rule: "If CV29 Bit 1 is set to 0, Bit 4 controls FL. In this mode there are 14 speed steps defined. If CV 29 Bit 1 is set to 0, bit 4 is used as an intermediate speed step."
    // FIXME: find better handling for Stop = 0b00000, Stop(I) = 0b10000, E-Stop = 0b00001, and E-Stop(I) = 0b10001

    if (speed > 31) {
        qCWarning(logger<Request>(), "Speed value out of range [0..31]");
        return {};
    }

    return setSpeed14(address, speed >> 1, direction, speed & 1);
}

Request Request::setSpeed126(quint16 address, quint8 speed, dcc::Direction direction)
{
    // FIXME: find better handling for Stop = 0b0000, and E-Stop = 0b0001

    if (speed > 127) {
        qCWarning(logger<Request>(), "Speed value out of range [0..127]");
        return {};
    }

    switch (direction) {
    case dcc::Direction::Forward:
        return Request{multiFunctionPacket(address, 0b001'11111, 0x80 | speed)};
    case dcc::Direction::Reverse:
        return Request{multiFunctionPacket(address, 0b001'11111, 0x00 | speed)};
    case dcc::Direction::Unknown:
        break;
    }

    // FIXME: 0b001'11110 restricts speed, with data bit 7 indicating enable/disable

    qCWarning(logger<Request>(), "Invalid direction");
    return {};
}

Request Request::setFunctions(quint16 address, dcc::FunctionGroup group, quint8 functions)
{
    switch (group) {
    case dcc::FunctionGroup::None:
        break;

    case dcc::FunctionGroup::Group1:
        if (functions > 0x1f) {
            qCWarning(logger<Request>(), "Functions value out of range for function group 1");
            break;
        }

        return Request{multiFunctionPacket(address, 0b100'00000 | functions)};

    case dcc::FunctionGroup::Group2:
        if (functions > 0x1f) {
            qCWarning(logger<Request>(), "Functions value out of range for function group 2");
            break;
        }

        return Request{multiFunctionPacket(address, 0b1011'0000 | functions)};

    case dcc::FunctionGroup::Group3:
        if (functions > 0x1f) {
            qCWarning(logger<Request>(), "Functions value out of range for function group 3");
            break;
        }

        return Request{multiFunctionPacket(address, 0b1010'0000 | functions)};

    case dcc::FunctionGroup::Group4:
        return Request{multiFunctionPacket(address, 0b1101'1110, functions)};
    case dcc::FunctionGroup::Group5:
        return Request{multiFunctionPacket(address, 0b1101'1111, functions)};
    case dcc::FunctionGroup::Group6:
        return Request{multiFunctionPacket(address, 0b1101'1000, functions)};
    case dcc::FunctionGroup::Group7:
        return Request{multiFunctionPacket(address, 0b1101'1001, functions)};
    case dcc::FunctionGroup::Group8:
        return Request{multiFunctionPacket(address, 0b1101'1010, functions)};
    case dcc::FunctionGroup::Group9:
        return Request{multiFunctionPacket(address, 0b1101'1011, functions)};
    case dcc::FunctionGroup::Group10:
        return Request{multiFunctionPacket(address, 0b1101'1100, functions)};
    }

    return {};
}

Request Request::verifyBit(quint16 variable, bool value, quint8 position)
{
    const auto addrHigh = (variable - 1) >> 8;
    const auto addrLow = (variable - 1) & 255;

    std::array<quint8, 4> packet = {
        static_cast<quint8>(0x78 | addrHigh),
        static_cast<quint8>(addrLow),
        static_cast<quint8>(0xe0 | (value ? 8 : 0) | position),
        0
    };

    updateChecksum(packet);

    return Request{{reinterpret_cast<char *>(packet.data()), packet.size()}};
}

Request Request::verifyByte(quint16 variable, quint8 value)
{
    const auto addrHigh = (variable - 1) >> 8;
    const auto addrLow = (variable - 1) & 255;

    std::array<quint8, 4> packet = {
        static_cast<quint8>(0x74 | addrHigh),
        static_cast<quint8>(addrLow),
        value, 0
    };

    updateChecksum(packet);

    return Request{{reinterpret_cast<char *>(packet.data()), packet.size()}};
}

Request Request::writeByte(quint16 variable, quint8 value)
{
    const auto addrHigh = (variable - 1) >> 8;
    const auto addrLow = (variable - 1) & 255;

    std::array<quint8, 4> packet = {
        static_cast<quint8>(0x7c | addrHigh),
        static_cast<quint8>(addrLow),
        value, 0
    };

    updateChecksum(packet);

    return Request{{reinterpret_cast<char *>(packet.data()), packet.size()}};
}

QDebug operator<<(QDebug debug, Request request)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(request)>{debug};
    const auto start = (request.hasExtendedAddress() ? 2 : 1);
    const auto data = request.toByteArray();

    debug << "address=" << request.address();

    if ((data.size() - start) == 2) {
        const auto byte = static_cast<quint8>(data[start]);
        const auto command = (byte >> 5) & 0x07;
        const auto args = (byte & 0x1f);

        debug << qSetPadChar('0'_L1) ;
        debug << ", command=0b" << Qt::bin << qSetFieldWidth(3) << command;
        debug << ", args=0b" << Qt::bin << qSetFieldWidth(5) << args;
        debug << qSetFieldWidth(0);
    } else {
        const auto command = static_cast<quint8>(data[start]);
        const auto args = data.mid(start + 1, data.size() - start - 2);

        debug << qSetPadChar('0'_L1) ;
        debug << ", command=0b" << Qt::bin << qSetFieldWidth(8) << command;
        debug << ", args=" << qSetFieldWidth(0) << args.toHex(' ');
    }

    return debug;
}

} // namespace lmrs::core::dcc
