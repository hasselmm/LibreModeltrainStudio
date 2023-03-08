#ifndef LMRS_CORE_USERLITERALS_H
#define LMRS_CORE_USERLITERALS_H

#include <QLatin1String>

#include <chrono>

class QRegularExpression;
class QRegularExpressionMatch;
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
QRegularExpression operator ""_regex(const char *str, size_t len);

namespace internal {

template<class Tag>
struct RegularExpressionLiteralBase
{
    [[nodiscard]] bool operator==(const RegularExpressionLiteralBase &rhs) const noexcept = default;

    [[nodiscard]] QRegularExpressionMatch match(QString subject) const noexcept;
    [[nodiscard]] QRegularExpression compile(Qt::CaseSensitivity cs = Qt::CaseInsensitive) const noexcept;
    [[nodiscard]] operator QRegularExpression() const noexcept;
    [[nodiscard]] size_t qHash(size_t seed = 0) const noexcept;

    QLatin1StringView pattern;
};

template<class Tag>
[[nodiscard]] inline size_t qHash(const RegularExpressionLiteralBase<Tag> &literal, size_t seed = 0) noexcept
{
    return literal.qHash(seed);
}

} // namespace internal

struct RegularExpressionLiteral : public internal::RegularExpressionLiteralBase<struct RegularExpressionLiteralTag>
{
    RegularExpressionLiteral operator+(RegularExpressionLiteral rhs) const noexcept;
};

struct WildcardLiteral : public internal::RegularExpressionLiteralBase<struct WildcardLiteralTag>
{
};

constexpr RegularExpressionLiteral operator ""_refrag(const char *str, size_t len)
{
    return {QLatin1StringView{str, static_cast<int>(len)}};
}

constexpr WildcardLiteral operator ""_wildcard(const char *str, size_t len)
{
    return {QLatin1StringView{str, static_cast<int>(len)}};
}

} // namespace lmrs::core::literals

using namespace lmrs::core::literals;

#endif // LMRS_CORE_USERLITERALS_H
