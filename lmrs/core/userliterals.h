#ifndef LMRS_CORE_USERLITERALS_H
#define LMRS_CORE_USERLITERALS_H

#include <QLatin1String>

#include <chrono>

class QUrl;

namespace lmrs::core::literals {

using namespace std::chrono_literals;

constexpr auto operator ""_BV(const char *str, size_t len)
{
    return QByteArrayView{str, static_cast<int>(len)};
}

constexpr auto operator ""_L1(const char *str, size_t len)
{
    return QLatin1StringView{str, static_cast<int>(len)};
}

constexpr auto operator ""_L1(char value)
{
    return QLatin1Char{value};
}

constexpr auto operator ""_U(unsigned long long codepoint)
{
    return QChar{static_cast<char16_t>(codepoint)};
}

inline auto operator ""_hex(const char *str, size_t len)
{
    return QByteArray::fromHex(QByteArray::fromRawData(str, static_cast<qsizetype>(len)));
}

constexpr auto operator ""_u8(unsigned long long value)
{
    return static_cast<quint8>(value);
}

constexpr auto operator""_u16(unsigned long long value)
{
    return static_cast<quint16>(value);
}

constexpr auto operator""_size(unsigned long long value)
{
    return static_cast<qsizetype>(value);
}

QUrl operator ""_url(const char *str, size_t len);

} // namespace lmrs::core::literals

using namespace lmrs::core::literals;

#endif // LMRS_CORE_USERLITERALS_H
