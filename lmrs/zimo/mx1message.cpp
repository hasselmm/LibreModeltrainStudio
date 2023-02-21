#include "mx1message.h"

#include <lmrs/core/logging.h>
#include <lmrs/core/userliterals.h>

#include <QtEndian>

namespace lmrs::zimo::mx1 {

namespace dcc = core::dcc;

namespace {

constexpr auto s_commandStationRequest =
        Request::Type::Request | Request::Format::Short
        | Request::Source::Host | Request::Target::CommandStation;

constexpr quint16 crc16(QByteArrayView data, quint16 init, const std::array<quint16, 256> &lut)
{
    auto crc = init;

    for (auto ch: data) {
        const auto b = static_cast<quint8>(ch);
        // xor-in next input byte into MSB of crc, that's our new intermediate divident
        const auto pos = static_cast<quint8>((crc >> 8) ^ b); // equal: ((crc ^ (b << 8)) >> 8)
        // shift out the MSB used for division per lookuptable and xor with the remainder
        crc = static_cast<quint16>(crc << 8) ^ lut[pos];
    }

    return crc;
}

constexpr quint8 crc8(QByteArrayView data, quint8 init, const std::array<quint8, 256> &lut)
{
    auto crc = init;

    for (auto ch: data) {
        const auto b = static_cast<quint8>(ch);
        crc = lut[crc ^ b];
    }

    return crc;
}

template <typename T>
QByteArray toByteArray(T value)
{
    return {reinterpret_cast<const char *>(&value), sizeof(value)};
}

bool printHeader(QDebug &debug, const Message &message)
{
    if (Q_UNLIKELY(!message.isValid())) {
        debug << "INVALID";
        return false;
    }

    const auto checksumState = (message.hasValidChecksum() ? "valid" : "INVALID");

    debug.setVerbosity(QDebug::MinimumVerbosity);
    debug << "sequence="    << message.sequence()
          << ", "           << message.type()
          << ", "           << message.format()
          << ", "           << message.source()
          << ", "           << message.target()
          << ", "           << message.dataFlow()
          << ", checksum="  << checksumState;

    return true;
}

} // namespace

// =====================================================================================================================

Message Message::fromData(QByteArray data, SequenceNumber sequence)
{
    data.prepend(static_cast<char>(sequence.value));
    return fromFrame(data + checksum(data));
}

Message Message::fromFrame(QByteArray frame)
{
    return {std::move(frame)};
}

QByteArray Message::toData() const noexcept
{
    switch (format()) {
    case Format::Short:
        return m_frame.mid(1, m_frame.size() - 2); // remove sequence id at front, and checksum at back
    case Format::Long:
        return m_frame.mid(1, m_frame.size() - 3); // remove sequence id at front, and checksum at back
    }

    Q_UNREACHABLE(); // softassert
    return {};
}

quint8 Message::headerSize() const noexcept
{
    switch (format()) {
    case Format::Short:
        return ShortHeaderSize;

    case Format::Long:
        return data(3) & 15;
    }

    Q_UNREACHABLE(); // softassert
    return {};
}

Message::DataFlow Message::dataFlow() const noexcept
{
    switch (format()) {
    case Format::Short:
        return DataFlow::EndOfData;

    case Format::Long:
        return DataFlow{static_cast<quint8>(data(3) & 0xc0)};
    }

    Q_UNREACHABLE(); // softassert
    return {};
}

bool Message::isValid() const noexcept
{
    return m_frame.size() >= ShortHeaderSize;
}

Message::SequenceNumber Message::nextSequence() noexcept
{
    static auto nextSequence = SequenceNumber{1};
    return nextSequence++;
}

bool Message::hasValidChecksum() const noexcept
{
    return actualChecksum() == expectedChecksum();
}

quint16 Message::actualChecksum() const noexcept
{
    switch (format()) {
    case Format::Short:
        return static_cast<quint8>(m_frame.back());

    case Format::Long:
        return qFromBigEndian<quint16>(m_frame.end() - 2);
    }

    Q_UNREACHABLE(); // softassert
    return {};
}

quint16 Message::expectedChecksum() const noexcept
{
    switch (format()) {
    case Format::Short:
        return checksum8({m_frame.constData(), m_frame.size() - 1});

    case Format::Long:
        return checksum16({m_frame.constData(), m_frame.size() - 2});
    }

    Q_UNREACHABLE(); // softassert
    return {};
}

// ---------------------------------------------------------------------------------------------------------------------

constexpr quint16 Message::checksum16(QByteArrayView data)
{
    return crc16(std::move(data), 0xffff, {
                     0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
                     0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
                     0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
                     0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
                     0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
                     0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
                     0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
                     0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
                     0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
                     0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
                     0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
                     0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
                     0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
                     0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
                     0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
                     0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
                     0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
                     0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
                     0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
                     0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
                     0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
                     0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
                     0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
                     0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
                     0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
                     0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
                     0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
                     0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
                     0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
                     0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
                     0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
                     0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
                 });
}

constexpr quint8 Message::checksum8(QByteArrayView data)
{
    // CRC8 for polynomial x^8 + x^5 + x^4 + 1 = 0x31, reflected
    // http://www.sunshine2k.de/coding/javascript/crc/crc_js.html

    return crc8(std::move(data), 0xff, {
                    0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83, 0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41,
                    0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e, 0x5f, 0x01, 0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc,
                    0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0, 0xe1, 0xbf, 0x5d, 0x03, 0x80, 0xde, 0x3c, 0x62,
                    0xbe, 0xe0, 0x02, 0x5c, 0xdf, 0x81, 0x63, 0x3d, 0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff,
                    0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5, 0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59, 0x07,
                    0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58, 0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a,
                    0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8, 0xe6, 0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24,
                    0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b, 0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7, 0xb9,
                    0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f, 0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd,
                    0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92, 0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0x0e, 0x50,
                    0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c, 0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0, 0xee,
                    0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d, 0xef, 0xb1, 0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73,
                    0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49, 0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b,
                    0x57, 0x09, 0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4, 0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16,
                    0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a, 0x2b, 0x75, 0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8,
                    0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7, 0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35,
                });
}

QByteArray Message::checksum(QByteArrayView data)
{
    Q_ASSERT(data.size() >= 3); // softassert
    Q_ASSERT(data.size() <= 15); // softassert

    switch (masked<Format>(static_cast<quint8>(data[1]))) {
    case Format::Short:
        return toByteArray(checksum8(std::move(data)));

    case Format::Long:
        return toByteArray(qToBigEndian(checksum16(std::move(data))));
    }

    Q_UNREACHABLE(); // softassert
    return {};

    // only can validate private members within members

    static_assert(Message::checksum8({"\x4c\x00\x01", 0}) == 255);
    static_assert(Message::checksum8({"\x4c\x00\x01", 1}) == 208);
    static_assert(Message::checksum8({"\x4c\x00\x01", 2}) == 87);
    static_assert(Message::checksum8({"\x4c\x00\x01", 3}) == 6);

    static_assert(Message::checksum8({"\x86\x00\x01", 3}) == 63);

    static_assert(Message::checksum8({"\x82\x10\x02\x02", 4}) == 0x52);

    static_assert(Message::checksum16({"\xa6\xff\x0d\x05\x03\x00\x00\x04\x00\x00\x01\x00\x00\x54\x09"
                                       "\x0b\x14\x16\x00\x27\x00\x00\x00\x00\x00\x01\x92\xb1\x93\x63", 28}) == 0x9363);
    static_assert(Message::checksum16({"\x94\xff\x0d\x05\x02\x00\x00\x04\x00\x00\x01\x00\x00\x54\x09"
                                       "\x0b\x14\x16\x00\x27\x00\x00\x00\x00\x00\x01\x92\xb1\x56\x4e", 28}) == 0x564e);
}

// =====================================================================================================================

Request Request::reset() noexcept
{
    return fromData(static_cast<quint8>(s_commandStationRequest),
                    static_cast<quint8>(Code::Reset));
}

Request Request::powerControl(PowerControl control)
{
    return fromData(static_cast<quint8>(s_commandStationRequest),
                    static_cast<quint8>(Code::PowerControl),
                    static_cast<quint8>(control));
}

Request Request::vehicleControl(dcc::VehicleAddress address, dcc::Speed speed,
                                dcc::Direction direction, dcc::FunctionState functions) noexcept
{
    enum class SpeedSystem : quint8 { Speed14 = 0x4, Speed28 = 0x8, Speed126 = 0xc };

    // speed-field
    // ===========
    // emergency stop:              0x80

    // data1
    // =====
    // overide speeds from signals: 0x80
    // use deceleration time "BZ":  0x20
    // use acceleration time "AZ":  0x10

    //    >S MX1 04 10 03 80 03 00 8c 00 00 00 00 f0    DCC#3
    //    >S MX1 05 10 03 80 03 00 8c 00 00 00 00 25    DCC#3
    //    >S MX1 06 10 03 80 03 00 9c 00 00 00 00 3f    DCC#3: F0
    //    >S MX1 08 10 03 80 03 00 9c 01 00 00 00 82    DCC#3: F0, F1
    //    >S MX1 09 10 03 80 03 00 9c 03 00 00 00 50    DCC#3: F0, F1-F2
    //    >S MX1 0a 10 03 80 03 00 9c 07 00 00 00 38    DCC#3: F0, F1-F3
    //    >S MX1 0b 10 03 80 03 00 9c 0f 00 00 00 f1    DCC#3: F0, F1-F4
    //    >S MX1 0c 10 03 80 03 00 9c 1f 00 00 00 98    DCC#3: F0, F1-F5
    //    >S MX1 0c 10 03 80 03 00 9c 3f 00 00 00 98    DCC#3: F0, F1-F6
    //    >S MX1 0d 10 03 80 03 00 9c 7f 00 00 00 ad    DCC#3: F0, F1-F7
    //    >S MX1 0e 10 03 80 03 00 9c ff 00 00 00 12    DCC#3: F0, F1-F8
    //    >S MX1 0f 10 03 80 03 00 9c ff 01 00 00 6c    DCC#3: F0, F1-F8, F9
    //    >S MX1 10 10 03 80 03 00 9c ff 03 00 00 21    DCC#3: F0, F1-F8, F9-F10
    //    >S MX1 11 10 03 80 03 00 9c ff 07 00 00 6a    DCC#3: F0, F1-F8, F9-F11
    //    >S MX1 12 10 03 80 03 00 9c ff 0f 00 00 29    DCC#3: F0, F1-F8, F9-F12
    //    >S MX1 14 10 03 80 03 00 9c ff 0f 01 00 21    DCC#3: F0, F1-F8, F9-F12, F13
    //    >S MX1 15 10 03 80 03 00 9c ff 0f 03 00 65    DCC#3: F0, F1-F8, F9-F12, F13-F14
    //    >S MX1 16 10 03 80 03 00 9c ff 0f 07 00 38    DCC#3: F0, F1-F8, F9-F12, F13-F15
    //    >S MX1 17 10 03 80 03 00 9c ff 0f 0f 00 9b    DCC#3: F0, F1-F8, F9-F12, F13-F16
    //    >S MX1 18 10 03 80 03 00 9c ff 0f 1f 00 90    DCC#3: F0, F1-F8, F9-F12, F13-F17
    //    >S MX1 19 10 03 80 03 00 9c ff 0f 3f 00 84    DCC#3: F0, F1-F8, F9-F12, F13-F18
    //    >S MX1 1a 10 03 80 03 00 9c ff 0f 7f 00 79    DCC#3: F0, F1-F8, F9-F12, F13-F19
    //    >S MX1 1b 10 03 80 03 00 9c ff 0f ff 00 83    DCC#3: F0, F1-F8, F9-F12, F13-F20
    //    >S MX1 1c 10 03 80 03 00 9c ff 0f ff 01 c4    DCC#3: F0, F1-F8, F9-F12, F13-F20, F21
    //    >S MX1 1e 10 03 80 03 00 9c ff 0f ff 03 cb    DCC#3: F0, F1-F8, F9-F12, F13-F20, F21-F22
    //    >S MX1 1f 10 03 80 03 00 9c ff 0f ff 07 7f    DCC#3: F0, F1-F8, F9-F12, F13-F20, F21-F23
    //    >S MX1 20 10 03 80 03 00 9c ff 0f ff 0f 6c    DCC#3: F0, F1-F8, F9-F12, F13-F20, F21-F24
    //    >S MX1 21 10 03 80 03 00 9c ff 0f ff 1f 24    DCC#3: F0, F1-F8, F9-F12, F13-F20, F21-F25
    //    >S MX1 22 10 03 80 03 00 9c ff 0f ff 3f 61    DCC#3: F0, F1-F8, F9-F12, F13-F20, F21-F26
    //    >S MX1 24 10 03 80 03 00 9c ff 0f ff 7f eb    DCC#3: F0, F1-F8, F9-F12, F13-F20, F21-F27
    //    >S MX1 25 10 03 80 03 00 9c ff 0f ff ff b2    DCC#3: F0, F1-F8, F9-F12, F13-F20, F21-F28
    //    >S MX1 26 10 03 80 03 00 9c ff 0f ff ff d4
    //    >S MX1 27 10 03 80 03 00 9c ff 0f ff ff 01

    const auto [speedSystem, rawSpeed] = [speed] {
        if (std::holds_alternative<dcc::Speed14>(speed))
            return std::make_tuple(SpeedSystem::Speed14, std::get<dcc::Speed14>(speed).count());
        if (std::holds_alternative<dcc::Speed28>(speed))
            return std::make_tuple(SpeedSystem::Speed28, std::get<dcc::Speed28>(speed).count());

        return std::make_tuple(SpeedSystem::Speed126, dcc::speedCast<dcc::Speed126>(speed).count());
    }();

    return fromData(static_cast<quint8>(s_commandStationRequest),
                    static_cast<quint8>(Code::VehicleControl),
                    static_cast<quint8>((63 & (address.value >> 8)) | AddressType::DCC),
                    static_cast<quint8>(255 & address.value),
                    static_cast<quint8>(rawSpeed),
                    static_cast<quint8>((direction == dcc::Direction::Reverse ? 0x20 : 0x00)
                                        | (functions.test(0) ? 0x10 : 0x00)
                                        | (speedSystem)),
                    static_cast<quint8>(255 & (functions >> 1).to_ulong()),
                    static_cast<quint8>(15  & (functions >> 9).to_ulong()),
                    static_cast<quint8>(255 & (functions >> 13).to_ulong()),
                    static_cast<quint8>(255 & (functions >> 21).to_ulong()));
}

Request Request::queryVehicle(dcc::VehicleAddress address) noexcept
{
    return fromData(static_cast<quint8>(s_commandStationRequest),
                    static_cast<quint8>(Code::QueryVehicle),
                    static_cast<quint8>((63 & (address.value >> 8)) | AddressType::DCC),
                    static_cast<quint8>(255 & address.value));
}

Request Request::readVariable(dcc::VehicleAddress address, dcc::VariableIndex variable) noexcept
{
    return fromData(static_cast<quint8>(s_commandStationRequest),
                    static_cast<quint8>(Code::VariableControl),
                    static_cast<quint8>((63 & (address.value >> 8)) | AddressType::DCC),
                    static_cast<quint8>(255 & address.value),
                    static_cast<quint8>(255 & (variable.value >> 8)),
                    static_cast<quint8>(255 & variable.value));
}

Request Request::writeVariable(dcc::VehicleAddress address, dcc::VariableIndex variable, dcc::VariableValue value) noexcept
{
    return fromData(static_cast<quint8>(s_commandStationRequest),
                    static_cast<quint8>(Code::VariableControl),
                    static_cast<quint8>((63 & (address.value >> 8)) | AddressType::DCC),
                    static_cast<quint8>(255 & address.value),
                    static_cast<quint8>(255 & (variable.value >> 8)),
                    static_cast<quint8>(255 & variable.value),
                    static_cast<quint8>(value));
}

Request Request::serialToolInfo(SerialToolAction action, SerialTool tool)
{
    return fromData(static_cast<quint8>(s_commandStationRequest),
                    static_cast<quint8>(Code::SerialToolInfo),
                    static_cast<quint8>(tool),
                    static_cast<quint8>(action));
}

Request Request::startCommunication(SerialTool tool)
{
    return serialToolInfo(SerialToolAction::StartCommunication, tool);
}

Request Request::refreshCommunication(SerialTool tool)
{
    return serialToolInfo(SerialToolAction::RefreshCommunication, tool);
}

Request Request::stopCommunication(SerialTool tool)
{
    return serialToolInfo(SerialToolAction::RefreshCommunication, tool);
}

Request Request::queryStationStatus(Target target)
{
    return fromData(static_cast<quint8>(s_commandStationRequest),
                    static_cast<quint8>(Code::QueryStationStatus),
                    static_cast<quint8>(target));
}

Request Request::queryStationEquipment(Target target)
{
    return fromData(static_cast<quint8>(s_commandStationRequest),
                    static_cast<quint8>(Code::QueryStationEquipment),
                    static_cast<quint8>(target));
}

// =====================================================================================================================

Request::Code Response::requestCode() const noexcept
{
    return Request::Code{data(2)};
}

Message::SequenceNumber Response::requestSequence() const noexcept
{
    switch (format()) {
    case Format::Short:
        return data(3);

    case Format::Long:
        return data(4);
    }

    return 0;
}

Response::Status Response::status() const noexcept
{
    if (frameSize() == 6)
        return Status{data(4)};

    return Status::Unknown;
}

qsizetype Response::dataSize() const noexcept
{

    switch (format()) {
    case Format::Short:
        return frameSize() - 5;

    case Format::Long:
        return frameSize() - 6;
    }

    return 0;
}

Response Response::secondaryAcknowledgement(SequenceNumber requestSequence, Request::Code requestCode)
{
    constexpr auto s_secondaryAcknowledgement =
            Request::Type::SecondaryAcknowledgement | Request::Format::Short
            | Request::Source::Host | Request::Target::CommandStation;

    return fromData(static_cast<quint8>(s_secondaryAcknowledgement),
                    static_cast<quint8>(requestCode),
                    static_cast<quint8>(requestSequence));
}

// ---------------------------------------------------------------------------------------------------------------------

QVersionNumber StationEquipmentReponse::makeVersion(int major, int minor, int micro) const noexcept
{
    if (micro != 0)
        return QVersionNumber{major, minor, micro};
    if (major != 0 || minor != 0)
        return QVersionNumber{major, minor};

    return {};
}

// =====================================================================================================================

QDebug operator<<(QDebug debug, const Message &message)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(message)>{debug};

    if (Q_LIKELY(message.isValid())) {
        switch (message.type()) {
        case Message::Type::Request:
            return debug << Request{message};

        case Message::Type::PrimaryResponse:
        case Message::Type::SecondaryResponse:
        case Message::Type::SecondaryAcknowledgement:
            return debug << Response{message};
        }
    }

    return debug << "INVALID";
}

QDebug operator<<(QDebug debug, const Request &request)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(request)>{debug};

    if (!printHeader(debug, request))
        return debug;

    return debug << ", " << request.code();
}

QDebug operator<<(QDebug debug, const Response &response)
{
    const auto prettyPrinter = core::PrettyPrinter<decltype(response)>{debug};

    if (!printHeader(debug, response))
        return debug;

    debug << ", " << response.status()
          << ", request=("
          << "sequence=" << response.requestSequence()
          << ", " << response.requestCode()
          << ")";

//    switch (response.requestCode()) {
//    case Request::Code::
//    }

    return debug;
}

} // namespace lmrs::zimo::mx1
